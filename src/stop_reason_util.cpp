/* The University of Illinois/NCSA
   Open Source License (NCSA)

   Copyright (c) 2025, Advanced Micro Devices, Inc. All rights reserved.

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

#include "stop_reason_util.h"

std::string
get_stop_reason_string (
    std::underlying_type_t<amd_dbgapi_wave_stop_reasons_t> stop_reason)
{
  std::string stop_reason_str = "";
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
          case AMD_DBGAPI_WAVE_STOP_REASON_ADDRESS_ERROR:
            return "ADDRESS_ERROR";
          case AMD_DBGAPI_WAVE_STOP_REASON_ILLEGAL_INSTRUCTION:
            return "ILLEGAL_INSTRUCTION";
          case AMD_DBGAPI_WAVE_STOP_REASON_ECC_ERROR:
            return "ECC_ERROR";
          case AMD_DBGAPI_WAVE_STOP_REASON_FATAL_HALT:
            return "FATAL_HALT";
#if AMD_DBGAPI_VERSION_MAJOR == 0 && AMD_DBGAPI_VERSION_MINOR < 58
          case AMD_DBGAPI_WAVE_STOP_REASON_RESERVED:
            return "RESERVED";
#endif
          }
        return "";
      }(static_cast<amd_dbgapi_wave_stop_reasons_t> (one_bit));
  } while (stop_reason_bits);
  return stop_reason_str;
}

std::underlying_type_t<amd_dbgapi_exceptions_t>
resume_exceptions_for_stop_reasons (
    std::underlying_type_t<amd_dbgapi_wave_stop_reasons_t> stop_reason)
{
  auto stop_reason_bits{ stop_reason };

  std::underlying_type_t<amd_dbgapi_exceptions_t> resume_exceptions = 0;
  do
    {
      auto one_bit
          = stop_reason_bits ^ (stop_reason_bits & (stop_reason_bits - 1));
      stop_reason_bits ^= one_bit;

      switch (stop_reason)
        {
        case AMD_DBGAPI_WAVE_STOP_REASON_NONE:
        case AMD_DBGAPI_WAVE_STOP_REASON_DEBUG_TRAP:
          resume_exceptions |= AMD_DBGAPI_EXCEPTION_NONE;
          break;

        case AMD_DBGAPI_WAVE_STOP_REASON_BREAKPOINT:
        case AMD_DBGAPI_WAVE_STOP_REASON_WATCHPOINT:
        case AMD_DBGAPI_WAVE_STOP_REASON_ASSERT_TRAP:
        case AMD_DBGAPI_WAVE_STOP_REASON_TRAP:
          resume_exceptions |= AMD_DBGAPI_EXCEPTION_WAVE_TRAP;
          break;

        case AMD_DBGAPI_WAVE_STOP_REASON_SINGLE_STEP:
          /* Is this even possible?  */
          resume_exceptions |= AMD_DBGAPI_EXCEPTION_NONE;
          break;

        case AMD_DBGAPI_WAVE_STOP_REASON_FP_INPUT_DENORMAL:
        case AMD_DBGAPI_WAVE_STOP_REASON_FP_DIVIDE_BY_0:
        case AMD_DBGAPI_WAVE_STOP_REASON_FP_OVERFLOW:
        case AMD_DBGAPI_WAVE_STOP_REASON_FP_UNDERFLOW:
        case AMD_DBGAPI_WAVE_STOP_REASON_FP_INEXACT:
        case AMD_DBGAPI_WAVE_STOP_REASON_FP_INVALID_OPERATION:
        case AMD_DBGAPI_WAVE_STOP_REASON_INT_DIVIDE_BY_0:
          resume_exceptions |= AMD_DBGAPI_EXCEPTION_WAVE_MATH_ERROR;
          break;

        case AMD_DBGAPI_WAVE_STOP_REASON_MEMORY_VIOLATION:
          resume_exceptions |= AMD_DBGAPI_EXCEPTION_WAVE_MEMORY_VIOLATION;
          break;

        case AMD_DBGAPI_WAVE_STOP_REASON_ADDRESS_ERROR:
          resume_exceptions |= AMD_DBGAPI_EXCEPTION_WAVE_ADDRESS_ERROR;
          break;

        case AMD_DBGAPI_WAVE_STOP_REASON_ILLEGAL_INSTRUCTION:
          resume_exceptions |= AMD_DBGAPI_EXCEPTION_WAVE_ILLEGAL_INSTRUCTION;
          break;

        case AMD_DBGAPI_WAVE_STOP_REASON_ECC_ERROR:
        case AMD_DBGAPI_WAVE_STOP_REASON_FATAL_HALT:
          resume_exceptions |= AMD_DBGAPI_EXCEPTION_WAVE_ABORT;
          break;

#if AMD_DBGAPI_VERSION_MAJOR == 0 && AMD_DBGAPI_VERSION_MINOR < 58
        case AMD_DBGAPI_WAVE_STOP_REASON_RESERVED:
          break;
#endif
        }
  } while (stop_reason_bits != 0);

  return resume_exceptions;
}
