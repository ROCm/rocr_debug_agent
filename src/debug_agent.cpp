/* The University of Illinois/NCSA
   Open Source License (NCSA)

   Copyright (c) 2020, Advanced Micro Devices, Inc. All rights reserved.

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to
   deal with the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

    - Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimers.
    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimers in
      the documentation and/or other materials provided with the distribution.
    - Neither the names of Advanced Micro Devices, Inc,
      nor the names of its contributors may be used to endorse or promote
      products derived from this Software without specific prior written
      permission.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS WITH THE SOFTWARE.  */

#include "code_object.h"
#include "debug.h"
#include "logging.h"

#include <amd-dbgapi/amd-dbgapi.h>
#include <hsa/hsa.h>
#include <hsa/hsa_api_trace.h>
#include <hsa/hsa_ext_amd.h>

#include <dlfcn.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#define DBGAPI_CHECK(expr)                                                    \
  do                                                                          \
    {                                                                         \
      if (amd_dbgapi_status_t status = (expr);                                \
          status != AMD_DBGAPI_STATUS_SUCCESS)                                \
        agent_error ("%s:%d: %s failed (rc=%d)", __FILE__, __LINE__, #expr,   \
                     status);                                                 \
    }                                                                         \
  while (false)

using namespace amd::debug_agent;
using namespace std::string_literals;

namespace
{
std::optional<std::string> g_code_objects_dir;
bool g_all_wavefronts{ false };

static amd_dbgapi_callbacks_t dbgapi_callbacks = {
  /* allocate_memory.  */
  .allocate_memory = malloc,

  /* deallocate_memory.  */
  .deallocate_memory = free,

  /* get_os_pid.  */
  .get_os_pid =
      [] (amd_dbgapi_client_process_id_t client_process_id, pid_t *pid) {
        *pid = getpid ();
        return AMD_DBGAPI_STATUS_SUCCESS;
      },

  /* set_breakpoint callback.  */
  .insert_breakpoint =
      [] (amd_dbgapi_client_process_id_t client_process_id,
          amd_dbgapi_global_address_t address,
          amd_dbgapi_breakpoint_id_t breakpoint_id) {
        return AMD_DBGAPI_STATUS_SUCCESS;
      },

  /* remove_breakpoint callback.  */
  .remove_breakpoint =
      [] (amd_dbgapi_client_process_id_t client_process_id,
          amd_dbgapi_breakpoint_id_t breakpoint_id) {
        return AMD_DBGAPI_STATUS_SUCCESS;
      },

  /* log_message callback.  */
  .log_message =
      [] (amd_dbgapi_log_level_t level, const char *message) {
        agent_out << "rocm-dbgapi: " << message << std::endl;
      }
};

std::string
hex_string (const std::vector<uint8_t> &value)
{
  std::string value_string;
  value_string.reserve (2 * value.size ());

  for (size_t pos = value.size (); pos > 0; --pos)
    {
      static constexpr char hex_digits[] = "0123456789abcdef";
      value_string.push_back (hex_digits[value[pos - 1] >> 4]);
      value_string.push_back (hex_digits[value[pos - 1] & 0xF]);
    }

  return value_string;
}

std::string
register_value_string (const std::string &register_type,
                       const std::vector<uint8_t> &register_value)
{
  /* handle vector types..  */
  if (size_t pos = register_type.find_last_of ('['); pos != std::string::npos)
    {
      const std::string element_type = register_type.substr (0, pos);
      const size_t element_count = std::stoi (register_type.substr (pos + 1));
      const size_t element_size = register_value.size () / element_count;

      agent_assert ((register_value.size () % element_size) == 0);

      std::stringstream ss;
      for (size_t i = 0; i < element_count; ++i)
        {
          if (i != 0)
            ss << " ";
          ss << "[" << i << "] ";

          std::vector<uint8_t> element_value (
              &register_value[element_size * i],
              &register_value[element_size * (i + 1)]);

          ss << register_value_string (element_type, element_value);
        }
      return ss.str ();
    }

  return hex_string (register_value);
}

void
print_registers (amd_dbgapi_wave_id_t wave_id)
{
  amd_dbgapi_architecture_id_t architecture_id;
  DBGAPI_CHECK (
      amd_dbgapi_wave_get_info (wave_id, AMD_DBGAPI_WAVE_INFO_ARCHITECTURE,
                                sizeof (architecture_id), &architecture_id));

  size_t class_count;
  amd_dbgapi_register_class_id_t *register_class_ids;
  DBGAPI_CHECK (amd_dbgapi_architecture_register_class_list (
      architecture_id, &class_count, &register_class_ids));

  size_t register_count;
  amd_dbgapi_register_id_t *register_ids;
  DBGAPI_CHECK (
      amd_dbgapi_wave_register_list (wave_id, &register_count, &register_ids));

  auto hash = [] (const amd_dbgapi_register_id_t &id) {
    return std::hash<decltype (id.handle)>{}(id.handle);
  };
  auto equal_to = [] (const amd_dbgapi_register_id_t &lhs,
                      const amd_dbgapi_register_id_t &rhs) {
    return std::equal_to<decltype (lhs.handle)>{}(lhs.handle, rhs.handle);
  };
  std::unordered_set<amd_dbgapi_register_id_t, decltype (hash),
                     decltype (equal_to)>
      printed_registers (0, hash, equal_to);

  for (size_t i = 0; i < class_count; ++i)
    {
      amd_dbgapi_register_class_id_t register_class_id = register_class_ids[i];

      char *class_name_;
      DBGAPI_CHECK (amd_dbgapi_architecture_register_class_get_info (
          register_class_id, AMD_DBGAPI_REGISTER_CLASS_INFO_NAME,
          sizeof (class_name_), &class_name_));
      std::string class_name (class_name_);
      free (class_name_);

      /* Always print the "general" register class last.  */
      if (class_name == "general" && i < (class_count - 1))
        {
          register_class_ids[i--] = register_class_ids[class_count - 1];
          register_class_ids[class_count - 1] = register_class_id;
          continue;
        }

      agent_out << std::endl << class_name << " registers:";

      size_t last_register_size = 0;
      for (size_t j = 0, column = 0; j < register_count; ++j)
        {
          amd_dbgapi_register_id_t register_id = register_ids[j];

          /* Skip this register if is has already been printed as part of
             another register class.  */
          if (printed_registers.find (register_id) != printed_registers.end ())
            continue;

          amd_dbgapi_register_class_state_t state;
          DBGAPI_CHECK (amd_dbgapi_register_is_in_register_class (
              register_class_id, register_id, &state));

          if (state != AMD_DBGAPI_REGISTER_CLASS_STATE_MEMBER)
            continue;

          char *register_name_;
          DBGAPI_CHECK (amd_dbgapi_register_get_info (
              register_id, AMD_DBGAPI_REGISTER_INFO_NAME,
              sizeof (register_name_), &register_name_));
          std::string register_name (register_name_);
          free (register_name_);

          char *register_type_;
          DBGAPI_CHECK (amd_dbgapi_register_get_info (
              register_id, AMD_DBGAPI_REGISTER_INFO_TYPE,
              sizeof (register_type_), &register_type_));
          std::string register_type (register_type_);
          free (register_type_);

          size_t register_size;
          DBGAPI_CHECK (amd_dbgapi_register_get_info (
              register_id, AMD_DBGAPI_REGISTER_INFO_SIZE,
              sizeof (register_size), &register_size));

          std::vector<uint8_t> buffer (register_size);
          DBGAPI_CHECK (amd_dbgapi_read_register (
              wave_id, register_id, 0, register_size, buffer.data ()));

          const size_t num_register_per_line = 16 / register_size;

          if (register_size > sizeof (uint64_t) /* Registers larger than a
                                                   uint64_t are printed each
                                                   on a separate line.  */
              || register_size != last_register_size
              || (column++ % num_register_per_line) == 0)
            {
              agent_out << std::endl;
              column = 1;
            }

          last_register_size = register_size;

          agent_out << std::right << std::setfill (' ') << std::setw (16)
                    << (register_name + ": ")
                    << register_value_string (register_type, buffer);

          printed_registers.emplace (register_id);
        }

      agent_out << std::endl;
    }

  free (register_ids);
  free (register_class_ids);
}

void
print_local_memory (amd_dbgapi_wave_id_t wave_id)
{
  amd_dbgapi_process_id_t process_id;
  DBGAPI_CHECK (amd_dbgapi_wave_get_info (wave_id,
                                          AMD_DBGAPI_WAVE_INFO_PROCESS,
                                          sizeof (process_id), &process_id));

  amd_dbgapi_architecture_id_t architecture_id;
  DBGAPI_CHECK (
      amd_dbgapi_wave_get_info (wave_id, AMD_DBGAPI_WAVE_INFO_ARCHITECTURE,
                                sizeof (architecture_id), &architecture_id));

  amd_dbgapi_address_space_id_t local_address_space_id;
  DBGAPI_CHECK (amd_dbgapi_dwarf_address_space_to_address_space (
      architecture_id, 0x3 /* DW_ASPACE_AMDGPU_local */,
      &local_address_space_id));

  std::vector<uint32_t> buffer (1024);
  amd_dbgapi_segment_address_t base_address{ 0 };

  while (true)
    {
      size_t requested_size = buffer.size () * sizeof (buffer[0]);
      size_t size = requested_size;
      if (amd_dbgapi_read_memory (process_id, wave_id, 0,
                                  local_address_space_id, base_address, &size,
                                  buffer.data ())
          != AMD_DBGAPI_STATUS_SUCCESS)
        break;

      agent_assert ((size % sizeof (buffer[0])) == 0);
      buffer.resize (size / sizeof (buffer[0]));

      if (!base_address)
        agent_out << std::endl << "Local memory content:";

      for (size_t i = 0, column = 0; i < buffer.size (); ++i)
        {
          if ((column++ % 8) == 0)
            {
              agent_out << std::endl
                        << "    0x" << std::setfill ('0') << std::setw (4)
                        << (base_address + i * sizeof (buffer[0])) << ":";
              column = 1;
            }

          agent_out << " " << std::hex << std::setfill ('0') << std::setw (8)
                    << buffer[i];
        }

      base_address += size;

      if (size != requested_size)
        break;
    }

  if (base_address)
    agent_out << std::endl;
}

void
stop_all_wavefronts (amd_dbgapi_process_id_t process_id)
{
  using wave_handle_type_t = decltype (amd_dbgapi_wave_id_t::handle);
  std::unordered_set<wave_handle_type_t> already_stopped;
  std::unordered_set<wave_handle_type_t> waiting_to_stop;

  agent_log (log_level_t::info, "stopping all wavefronts");
  for (size_t iter = 0;; ++iter)
    {
      agent_log (log_level_t::info, "iteration %zu:", iter);

      while (true)
        {
          amd_dbgapi_event_id_t event_id;
          amd_dbgapi_event_kind_t kind;

          DBGAPI_CHECK (amd_dbgapi_process_next_pending_event (
              process_id, &event_id, &kind));

          if (event_id.handle == AMD_DBGAPI_EVENT_NONE.handle)
            break;

          if (kind == AMD_DBGAPI_EVENT_KIND_WAVE_STOP
              || kind == AMD_DBGAPI_EVENT_KIND_WAVE_COMMAND_TERMINATED)
            {
              amd_dbgapi_wave_id_t wave_id;
              DBGAPI_CHECK (amd_dbgapi_event_get_info (
                  event_id, AMD_DBGAPI_EVENT_INFO_WAVE, sizeof (wave_id),
                  &wave_id));

              agent_assert (waiting_to_stop.find (wave_id.handle)
                            != waiting_to_stop.end ());

              waiting_to_stop.erase (wave_id.handle);

              if (kind == AMD_DBGAPI_EVENT_KIND_WAVE_STOP)
                {
                  already_stopped.emplace (wave_id.handle);

                  agent_log (log_level_t::info, "wave_%ld is stopped",
                             wave_id.handle);
                }
              else /* kind == AMD_DBGAPI_EVENT_KIND_COMMAND_TERMINATED */
                {
                  agent_log (log_level_t::info,
                             "wave_%ld terminated while stopping",
                             wave_id.handle);
                }
            }

          DBGAPI_CHECK (amd_dbgapi_event_processed (event_id));
        }

      amd_dbgapi_wave_id_t *wave_ids;
      size_t wave_count;
      DBGAPI_CHECK (amd_dbgapi_process_wave_list (process_id, &wave_count,
                                                  &wave_ids, nullptr));

      /* Stop all waves that are still running.  */
      for (size_t i = 0; i < wave_count; ++i)
        {
          amd_dbgapi_wave_id_t wave_id = wave_ids[i];

          if (already_stopped.find (wave_id.handle) != already_stopped.end ())
            continue;

          /* Already requested to stop.  */
          if (waiting_to_stop.find (wave_id.handle) != waiting_to_stop.end ())
            {
              agent_log (log_level_t::info, "wave_%ld is still stopping",
                         wave_id.handle);
              continue;
            }

          amd_dbgapi_wave_state_t state;
          if (amd_dbgapi_status_t status = amd_dbgapi_wave_get_info (
                  wave_id, AMD_DBGAPI_WAVE_INFO_STATE, sizeof (state), &state);
              status == AMD_DBGAPI_STATUS_ERROR_INVALID_WAVE_ID)
            {
              /* The wave could have terminated since it was reported in the
                 last wave list.  Skip it.  */
              continue;
            }
          else if (status != AMD_DBGAPI_STATUS_SUCCESS)
            agent_error ("amd_dbgapi_wave_get_info failed (rc=%d)", status);

          if (state == AMD_DBGAPI_WAVE_STATE_STOP)
            {
              already_stopped.emplace (wave_ids[i].handle);

              agent_log (log_level_t::info, "wave_%ld is already stopped",
                         wave_id.handle);
              continue;
            }
          if (state == AMD_DBGAPI_WAVE_STATE_SINGLE_STEP)
            {
              /* The wave is single-stepping, it will stop and report an event
                 once the instruction execution is complete.  */
              agent_log (log_level_t::info, "wave_%ld is single-stepping",
                         wave_id.handle);
              continue;
            }

          if (amd_dbgapi_status_t status = amd_dbgapi_wave_stop (wave_id);
              status == AMD_DBGAPI_STATUS_ERROR_INVALID_WAVE_ID)
            {
              /* The wave could have terminated since it was reported in the
                 last wave list.  Skip it.  */
              continue;
            }
          else if (status != AMD_DBGAPI_STATUS_SUCCESS)
            agent_error ("amd_dbgapi_wave_stop failed (rc=%d)", status);

          agent_log (log_level_t::info,
                     "wave_%ld is running, sent stop request", wave_id.handle);

          waiting_to_stop.emplace (wave_id.handle);
        }

      free (wave_ids);

      if (!waiting_to_stop.size ())
        break;
    }

  agent_log (log_level_t::info, "all wavefronts are stopped");
}

void
print_wavefronts (bool all_wavefronts)
{
  /* This function is not thread-safe and not re-entrant.  */
  static std::mutex lock;
  if (!lock.try_lock ())
    return;
  /* Make sure the lock is released when this function returns.  */
  std::scoped_lock sl (std::adopt_lock, lock);

  DBGAPI_CHECK (amd_dbgapi_initialize (&dbgapi_callbacks));

  amd_dbgapi_process_id_t process_id;
  DBGAPI_CHECK (amd_dbgapi_process_attach (
      reinterpret_cast<amd_dbgapi_client_process_id_t> (&process_id),
      &process_id));

  /* Check the runtime state.  */
  while (true)
    {
      amd_dbgapi_event_id_t event_id;
      amd_dbgapi_event_kind_t event_kind;

      DBGAPI_CHECK (amd_dbgapi_process_next_pending_event (
          process_id, &event_id, &event_kind));

      if (event_kind == AMD_DBGAPI_EVENT_KIND_RUNTIME)
        {
          amd_dbgapi_runtime_state_t runtime_state;

          DBGAPI_CHECK (amd_dbgapi_event_get_info (
              event_id, AMD_DBGAPI_EVENT_INFO_RUNTIME_STATE,
              sizeof (runtime_state), &runtime_state));

          switch (runtime_state)
            {
            case AMD_DBGAPI_RUNTIME_STATE_LOADED_SUCCESS:
              break;

            case AMD_DBGAPI_RUNTIME_STATE_UNLOADED:
              agent_error ("invalid runtime state %d", runtime_state);

            case AMD_DBGAPI_RUNTIME_STATE_LOADED_ERROR_RESTRICTION:
              agent_error ("unable to enable GPU debugging due to a "
                           "restriction error");
              break;
            }
        }

      /* No more events.  */
      if (event_kind == AMD_DBGAPI_EVENT_KIND_NONE)
        break;

      DBGAPI_CHECK (amd_dbgapi_event_processed (event_id));
    }

  std::map<amd_dbgapi_global_address_t, code_object_t> code_object_map;

  amd_dbgapi_code_object_id_t *code_objects_id;
  size_t code_object_count;
  DBGAPI_CHECK (amd_dbgapi_process_code_object_list (
      process_id, &code_object_count, &code_objects_id, nullptr));

  for (size_t i = 0; i < code_object_count; ++i)
    {
      code_object_t code_object (code_objects_id[i]);

      code_object.open ();
      if (!code_object.is_open ())
        {
          agent_warning ("could not open code_object_%ld",
                         code_objects_id[i].handle);
          continue;
        }

      if (g_code_objects_dir && !code_object.save (*g_code_objects_dir))
        agent_warning ("could not save code object to %s",
                       g_code_objects_dir->c_str ());

      code_object_map.emplace (code_object.load_address (),
                               std::move (code_object));
    }
  free (code_objects_id);

  DBGAPI_CHECK (amd_dbgapi_process_set_progress (
      process_id, AMD_DBGAPI_PROGRESS_NO_FORWARD));

  DBGAPI_CHECK (amd_dbgapi_process_set_wave_creation (
      process_id, AMD_DBGAPI_WAVE_CREATION_STOP));

  if (all_wavefronts)
    stop_all_wavefronts (process_id);

  amd_dbgapi_wave_id_t *wave_ids;
  size_t wave_count;
  DBGAPI_CHECK (amd_dbgapi_process_wave_list (process_id, &wave_count,
                                              &wave_ids, nullptr));

  for (size_t i = 0; i < wave_count; ++i)
    {
      amd_dbgapi_wave_id_t wave_id = wave_ids[i];

      amd_dbgapi_wave_state_t state;
      DBGAPI_CHECK (amd_dbgapi_wave_get_info (
          wave_id, AMD_DBGAPI_WAVE_INFO_STATE, sizeof (state), &state));

      if (state != AMD_DBGAPI_WAVE_STATE_STOP)
        continue;

      std::underlying_type_t<amd_dbgapi_wave_stop_reasons_t> stop_reason;
      DBGAPI_CHECK (
          amd_dbgapi_wave_get_info (wave_id, AMD_DBGAPI_WAVE_INFO_STOP_REASON,
                                    sizeof (stop_reason), &stop_reason));

      amd_dbgapi_global_address_t pc;
      DBGAPI_CHECK (amd_dbgapi_wave_get_info (wave_id, AMD_DBGAPI_WAVE_INFO_PC,
                                              sizeof (pc), &pc));

      if (i)
        agent_out << std::endl;

      agent_out << "--------------------------------------------------------"
                << std::endl;

      agent_out << "wave_" << std::dec << wave_id.handle << ": pc=0x"
                << std::hex << pc;

      std::string stop_reason_str;
      auto stop_reason_bits{ stop_reason };
      do
        {
          /* Consume one bit from the stop reason.  */
          auto one_bit
              = stop_reason_bits ^ (stop_reason_bits & (stop_reason_bits - 1));
          stop_reason_bits ^= one_bit;

          if (!stop_reason_str.empty ())
            stop_reason_str += "|";

          stop_reason_str += [] (amd_dbgapi_wave_stop_reasons_t reason) {
            switch (reason)
              {
              case AMD_DBGAPI_WAVE_STOP_REASON_NONE:
                return "NONE";
              case AMD_DBGAPI_WAVE_STOP_REASON_BREAKPOINT:
                return "BREAKPOINT";
              case AMD_DBGAPI_WAVE_STOP_REASON_WATCHPOINT:
                return "WATCHPOINT";
              case AMD_DBGAPI_WAVE_STOP_REASON_SINGLE_STEP:
                return "SINGLE_STEP";
              case AMD_DBGAPI_WAVE_STOP_REASON_FP_INPUT_DENORMAL:
                return "FP_INPUT_DENORMAL";
              case AMD_DBGAPI_WAVE_STOP_REASON_FP_DIVIDE_BY_0:
                return "FP_DIVIDE_BY_0";
              case AMD_DBGAPI_WAVE_STOP_REASON_FP_OVERFLOW:
                return "FP_OVERFLOW";
              case AMD_DBGAPI_WAVE_STOP_REASON_FP_UNDERFLOW:
                return "FP_UNDERFLOW";
              case AMD_DBGAPI_WAVE_STOP_REASON_FP_INEXACT:
                return "FP_INEXACT";
              case AMD_DBGAPI_WAVE_STOP_REASON_FP_INVALID_OPERATION:
                return "FP_INVALID_OPERATION";
              case AMD_DBGAPI_WAVE_STOP_REASON_INT_DIVIDE_BY_0:
                return "INT_DIVIDE_BY_0";
              case AMD_DBGAPI_WAVE_STOP_REASON_DEBUG_TRAP:
                return "DEBUG_TRAP";
              case AMD_DBGAPI_WAVE_STOP_REASON_ASSERT_TRAP:
                return "ASSERT_TRAP";
              case AMD_DBGAPI_WAVE_STOP_REASON_TRAP:
                return "TRAP";
              case AMD_DBGAPI_WAVE_STOP_REASON_MEMORY_VIOLATION:
                return "MEMORY_VIOLATION";
              case AMD_DBGAPI_WAVE_STOP_REASON_APERTURE_VIOLATION:
                return "APERTURE_VIOLATION";
              case AMD_DBGAPI_WAVE_STOP_REASON_ILLEGAL_INSTRUCTION:
                return "ILLEGAL_INSTRUCTION";
              case AMD_DBGAPI_WAVE_STOP_REASON_ECC_ERROR:
                return "ECC_ERROR";
              case AMD_DBGAPI_WAVE_STOP_REASON_FATAL_HALT:
                return "FATAL_HALT";
#if AMD_DBGAPI_VERSION_MAJOR == 0 &&  AMD_DBGAPI_VERSION_MINOR < 58
              case AMD_DBGAPI_WAVE_STOP_REASON_RESERVED:
                return "RESERVED";
#endif
              }
            return "";
          }(static_cast<amd_dbgapi_wave_stop_reasons_t> (one_bit));
        }
      while (stop_reason_bits);

      agent_out << " (";
      if (stop_reason != AMD_DBGAPI_WAVE_STOP_REASON_NONE)
        agent_out << "stopped, reason: " << stop_reason_str;
      else
        agent_out << "running";
      agent_out << ")" << std::endl;

      print_registers (wave_id);
      print_local_memory (wave_id);

      /* Find the code object that contains this pc, and disassemble
         instructions around `pc`  */
      code_object_t *code_object_found{ nullptr };
      if (auto it = code_object_map.upper_bound (pc);
          it != code_object_map.begin ())
        if (auto &&[load_address, code_object] = *std::prev (it);
            (pc - load_address) <= code_object.mem_size ())
          code_object_found = &code_object;

      if (code_object_found)
        {
          amd_dbgapi_architecture_id_t architecture_id;
          DBGAPI_CHECK (amd_dbgapi_wave_get_info (
              wave_id, AMD_DBGAPI_WAVE_INFO_ARCHITECTURE,
              sizeof (architecture_id), &architecture_id));

          code_object_found->disassemble (architecture_id, pc);
        }
      else
        {
          /* TODO: Add disassembly even if we did not find a code object  */
        }

      if (stop_reason == AMD_DBGAPI_WAVE_STOP_REASON_NONE)
        {
          /* FIXME: What if the wave was single-stepping?  */
          DBGAPI_CHECK (
              amd_dbgapi_wave_resume (wave_id, AMD_DBGAPI_RESUME_MODE_NORMAL, AMD_DBGAPI_EXCEPTION_NONE));
        }
    }

  free (wave_ids);

  DBGAPI_CHECK (amd_dbgapi_process_set_wave_creation (
      process_id, AMD_DBGAPI_WAVE_CREATION_NORMAL));

  DBGAPI_CHECK (amd_dbgapi_process_set_progress (process_id,
                                                 AMD_DBGAPI_PROGRESS_NORMAL));

  DBGAPI_CHECK (amd_dbgapi_process_detach (process_id));
  DBGAPI_CHECK (amd_dbgapi_finalize ());
}

hsa_status_t
handle_system_event (const hsa_amd_event_t *event, void *data)
{
  if (event->event_type != HSA_AMD_GPU_MEMORY_FAULT_EVENT)
    return HSA_STATUS_SUCCESS;

  agent_out << "System event (HSA_AMD_GPU_MEMORY_FAULT_EVENT)" << std::endl;
  agent_out << "Faulting page: 0x" << std::hex
            << event->memory_fault.virtual_address << std::endl
            << std::endl;

  print_wavefronts (g_all_wavefronts);

  /* FIXME: We really should be returning to the ROCr and let it print more
     information then abort.  */
  abort ();
}

struct callback_and_data_t
{
  void (*callback) (hsa_status_t error_code, hsa_queue_t *source, void *data);
  void *data;
};

std::unordered_map<hsa_queue_t *, std::unique_ptr<callback_and_data_t>>
    original_callbacks;

void
handle_queue_error (hsa_status_t error_code, hsa_queue_t *queue, void *data)
{
  if (error_code == hsa_status_t (HSA_STATUS_ERROR_MEMORY_APERTURE_VIOLATION)
      || error_code == hsa_status_t (HSA_STATUS_ERROR_MEMORY_FAULT)
      || error_code == hsa_status_t (HSA_STATUS_ERROR_ILLEGAL_INSTRUCTION)
      || error_code == HSA_STATUS_ERROR_EXCEPTION)
    {
      const char *queue_error_str{ nullptr };

      hsa_status_t status = hsa_status_string (error_code, &queue_error_str);
      agent_assert (status == HSA_STATUS_SUCCESS);

      agent_out << "Queue error (" << queue_error_str << ")" << std::endl
                << std::endl;

      print_wavefronts (g_all_wavefronts);
    }

  /* Call the original callback.  */
  if (auto *original_callback = reinterpret_cast<callback_and_data_t *> (data);
      original_callback->callback)
    (*original_callback->callback) (error_code, queue,
                                    original_callback->data);
}

decltype (CoreApiTable::hsa_queue_create_fn) original_hsa_queue_create_fn = {};

hsa_status_t
queue_create (hsa_agent_t agent, uint32_t size, hsa_queue_type32_t type,
              void (*callback) (hsa_status_t status, hsa_queue_t *source,
                                void *data),
              void *data, uint32_t private_segment_size,
              uint32_t group_segment_size, hsa_queue_t **queue)
{
  auto original_callback = std::make_unique<callback_and_data_t> (
      callback_and_data_t{ callback, data });

  hsa_status_t status = (*original_hsa_queue_create_fn) (
      agent, size, type, handle_queue_error, original_callback.get (),
      private_segment_size, group_segment_size, queue);

  if (status == HSA_STATUS_SUCCESS)
    original_callbacks.emplace (*queue, std::move (original_callback));

  return status;
}

decltype (CoreApiTable::hsa_queue_destroy_fn) original_hsa_queue_destroy_fn
    = {};

hsa_status_t
queue_destroy (hsa_queue_t *queue)
{
  if (auto it = original_callbacks.find (queue);
      it != original_callbacks.end ())
    original_callbacks.erase (it);

  return (*original_hsa_queue_destroy_fn) (queue);
}

void
print_usage ()
{
  std::cerr << "ROCdebug-agent usage:" << std::endl;
  std::cerr << "  -a, --all                   "
               "Print all wavefronts."
            << std::endl;
  std::cerr << "  -s, --save-code-objects[=DIR]   "
               "Save all loaded code objects. If the directory"
            << std::endl
            << "                              "
               "is not specified, the code objects are saved in"
            << std::endl
            << "                              "
               "the current directory."
            << std::endl;
  std::cerr << "  -o, --output=FILE           "
               "Save the output in FILE. By default, the output"
            << std::endl
            << "                              "
               "is redirected to stderr."
            << std::endl;
  std::cerr << "  -d, --disable-linux-signals "
               "Disable installing a SIGQUIT signal handler, so"
            << std::endl
            << "                              "
               "that the default Linux handler may dump a core"
            << std::endl
            << "                              "
               "file."
            << std::endl;
  std::cerr << "  -l, --log-level={none|error|warning|info|verbose}"
            << std::endl
            << "                              "
               "Change the Debug Agent and Debugger API log"
            << std::endl
            << "                              "
               "level. The default log level is 'none'."
            << std ::endl;
  std::cerr << "  -h, --help                  "
               "Display a usage message and abort the process."
            << std::endl;

  abort ();
}

} /* namespace.  */

extern "C" bool __attribute__ ((visibility ("default")))
OnLoad (void *table, uint64_t runtime_version, uint64_t failed_tool_count,
        const char *const *failed_tool_names)
{
  bool disable_sigquit{ false };

  set_log_level (log_level_t::warning);

  std::istringstream args_stream;
  if (const char *env = ::getenv ("ROCM_DEBUG_AGENT_OPTIONS"))
    args_stream.str (env);

  std::vector<char *> args = { strdup ("rocm-debug-agent") };
  std::transform (
      std::istream_iterator<std::string> (args_stream),
      std::istream_iterator<std::string> (), std::back_inserter (args),
      [] (const std::string &str) { return strdup (str.c_str ()); });

  char *const *argv = const_cast<char *const *> (args.data ());
  int argc = args.size ();

  static struct option options[]
      = { { "all", no_argument, nullptr, 'a' },
          { "disable-linux-signals", no_argument, nullptr, 'd' },
          { "log-level", required_argument, nullptr, 'l' },
          { "output", required_argument, nullptr, 'o' },
          { "save-code-objects", optional_argument, nullptr, 's' },
          { "help", no_argument, nullptr, 'h' },
          { 0 } };

  /* We use getopt_long locally, so make sure to preserve and reset the
     global optind.  */
  int saved_optind = optind;
  optind = 1;

  while (int c = getopt_long (argc, argv, ":as::o:dl:h", options, nullptr))
    {
      if (c == -1)
        break;

      std::optional<std::string> argument;

      if (!optarg && optind < argc && *argv[optind] != '-')
        optarg = argv[optind++];

      if (optarg)
        argument.emplace (optarg);

      switch (c)
        {
        case 'a': /* -a or --all  */
          g_all_wavefronts = true;
          break;

        case 'd': /* -d or --disable-linux-signals  */
          disable_sigquit = true;
          break;

        case 'l': /* -l or --log-level  */
          if (!argument)
            print_usage ();

          if (argument == "none")
            set_log_level (log_level_t::none);
          else if (argument == "verbose")
            set_log_level (log_level_t::verbose);
          else if (argument == "info")
            set_log_level (log_level_t::info);
          else if (argument == "warning")
            set_log_level (log_level_t::warning);
          else if (argument == "error")
            set_log_level (log_level_t::error);
          else
            print_usage ();
          break;

        case 's': /* -s or --save-code-objects  */
          if (argument)
            {
              struct stat path_stat;
              if (stat (argument->c_str (), &path_stat) == -1
                  || !S_ISDIR (path_stat.st_mode))
                {
                  std::cerr
                      << "error: Cannot access code object save directory `"
                      << *argument << "'" << std::endl;
                  print_usage ();
                }

              g_code_objects_dir = *argument;
            }
          else
            {
              g_code_objects_dir = ".";
            }
          break;

        case 'o': /* -o or --output  */
          if (!argument)
            print_usage ();

          agent_out.open (*argument);
          if (!agent_out.is_open ())
            {
              std::cerr << "could not open `" << *argument << "'" << std::endl;
              abort ();
            }
          break;

        case '?': /* Unrecognized option  */
        case 'h': /* -h or --help */
        default:
          print_usage ();
        }
    }

  /* Restore the global optind.  */
  optind = saved_optind;

  std::for_each (args.begin (), args.end (), [] (char *str) { free (str); });

  if (!agent_out.is_open ())
    {
      agent_out.copyfmt (std::cerr);
      agent_out.clear (std::cerr.rdstate ());
      agent_out.basic_ios<char>::rdbuf (std::cerr.rdbuf ());
    }

  if (const char *env = ::getenv ("HSA_ENABLE_DEBUG");
      !env || ::strcmp (env, "1"))
    agent_error ("The environment variable 'HSA_ENABLE_DEBUG' must be set "
                 "to '1' to enable the debug agent");

  if (!disable_sigquit)
    {
      struct sigaction sig_action;

      memset (&sig_action, '\0', sizeof (sig_action));
      sigemptyset (&sig_action.sa_mask);

      sig_action.sa_sigaction = [] (int signal, siginfo_t *, void *) {
        agent_out << std::endl;
        print_wavefronts (true);
      };

      /* Install a SIGQUIT (Ctrl-\) handler.  */
      sig_action.sa_flags = SA_RESTART;
      sigaction (SIGQUIT, &sig_action, nullptr);
    }

  /* Intercept the queue_create & queue_destroy functions.  */
  CoreApiTable *core_table = reinterpret_cast<HsaApiTable *> (table)->core_;

  original_hsa_queue_create_fn = core_table->hsa_queue_create_fn;
  core_table->hsa_queue_create_fn = &queue_create;

  original_hsa_queue_destroy_fn = core_table->hsa_queue_destroy_fn;
  core_table->hsa_queue_destroy_fn = &queue_destroy;

  /* Install a system handler to report memory faults.  */
  return hsa_amd_register_system_event_handler (handle_system_event, table)
         == HSA_STATUS_SUCCESS;
}

extern "C" void __attribute__ ((visibility ("default"))) OnUnload () {}
