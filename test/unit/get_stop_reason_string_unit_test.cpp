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
#include <gtest/gtest.h>
#include <string>
#include <type_traits>

/* Unit tests. */
TEST (GetStopReasonStringTest, SingleReason)
{
  EXPECT_EQ (get_stop_reason_string (AMD_DBGAPI_WAVE_STOP_REASON_NONE),
             "NONE");
  EXPECT_EQ (get_stop_reason_string (AMD_DBGAPI_WAVE_STOP_REASON_DEBUG_TRAP),
             "DEBUG_TRAP");
  EXPECT_EQ (get_stop_reason_string (AMD_DBGAPI_WAVE_STOP_REASON_BREAKPOINT),
             "BREAKPOINT");
  EXPECT_EQ (get_stop_reason_string (AMD_DBGAPI_WAVE_STOP_REASON_FP_OVERFLOW),
             "FP_OVERFLOW");
  EXPECT_EQ (get_stop_reason_string (AMD_DBGAPI_WAVE_STOP_REASON_FATAL_HALT),
             "FATAL_HALT");
}

TEST (GetStopReasonStringTest, MultipleReasons)
{
  /* Order is determined by lowest bit first. */
  auto combined = AMD_DBGAPI_WAVE_STOP_REASON_DEBUG_TRAP
                  | AMD_DBGAPI_WAVE_STOP_REASON_BREAKPOINT;
  EXPECT_EQ (get_stop_reason_string (combined), "BREAKPOINT|DEBUG_TRAP");

  combined = AMD_DBGAPI_WAVE_STOP_REASON_FP_OVERFLOW
             | AMD_DBGAPI_WAVE_STOP_REASON_FP_UNDERFLOW
             | AMD_DBGAPI_WAVE_STOP_REASON_FP_INEXACT;
  EXPECT_EQ (get_stop_reason_string (combined),
             "FP_OVERFLOW|FP_UNDERFLOW|FP_INEXACT");
}

TEST (GetStopReasonStringTest, AllReasons)
{
  uint32_t all_reasons = AMD_DBGAPI_WAVE_STOP_REASON_NONE
                         | AMD_DBGAPI_WAVE_STOP_REASON_DEBUG_TRAP
                         | AMD_DBGAPI_WAVE_STOP_REASON_BREAKPOINT
                         | AMD_DBGAPI_WAVE_STOP_REASON_WATCHPOINT
                         | AMD_DBGAPI_WAVE_STOP_REASON_ASSERT_TRAP
                         | AMD_DBGAPI_WAVE_STOP_REASON_TRAP
                         | AMD_DBGAPI_WAVE_STOP_REASON_SINGLE_STEP
                         | AMD_DBGAPI_WAVE_STOP_REASON_FP_INPUT_DENORMAL
                         | AMD_DBGAPI_WAVE_STOP_REASON_FP_DIVIDE_BY_0
                         | AMD_DBGAPI_WAVE_STOP_REASON_FP_OVERFLOW
                         | AMD_DBGAPI_WAVE_STOP_REASON_FP_UNDERFLOW
                         | AMD_DBGAPI_WAVE_STOP_REASON_FP_INEXACT
                         | AMD_DBGAPI_WAVE_STOP_REASON_FP_INVALID_OPERATION
                         | AMD_DBGAPI_WAVE_STOP_REASON_INT_DIVIDE_BY_0
                         | AMD_DBGAPI_WAVE_STOP_REASON_MEMORY_VIOLATION
                         | AMD_DBGAPI_WAVE_STOP_REASON_ADDRESS_ERROR
                         | AMD_DBGAPI_WAVE_STOP_REASON_ILLEGAL_INSTRUCTION
                         | AMD_DBGAPI_WAVE_STOP_REASON_ECC_ERROR
                         | AMD_DBGAPI_WAVE_STOP_REASON_FATAL_HALT;
  /* The expected string will be all names joined by '|', in order of bit
  position. */
  std::string expected
      = "BREAKPOINT|WATCHPOINT|SINGLE_STEP|FP_INPUT_DENORMAL|FP_DIVIDE_BY_0|"
        "FP_OVERFLOW|FP_UNDERFLOW|FP_INEXACT|FP_INVALID_OPERATION|INT_DIVIDE_"
        "BY_0|DEBUG_TRAP|ASSERT_TRAP|TRAP|MEMORY_VIOLATION|ADDRESS_ERROR|"
        "ILLEGAL_INSTRUCTION|ECC_ERROR|FATAL_HALT";
  EXPECT_EQ (get_stop_reason_string (all_reasons), expected);
}

TEST (GetStopReasonStringTest, UnknownReason)
{
  /* If an unknown bit is set, it should be skipped (returns empty string for
  that bit). */
  uint32_t unknown = 0x40000; /* Not defined in enum. */
  EXPECT_EQ (get_stop_reason_string (unknown), "");
}
