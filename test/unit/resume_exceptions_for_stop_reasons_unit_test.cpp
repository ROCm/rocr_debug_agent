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

#include <gtest/gtest.h>
#include <type_traits>
#include "stop_reason_util.h"

// Unit tests
TEST(ProcessDbgapiStopReasonTest, NoneAndDebugTrap) {
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_NONE),
              AMD_DBGAPI_EXCEPTION_NONE);
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_DEBUG_TRAP),
              AMD_DBGAPI_EXCEPTION_NONE);
}

TEST(ProcessDbgapiStopReasonTest, WaveTrapReasons) {
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_BREAKPOINT),
              AMD_DBGAPI_EXCEPTION_WAVE_TRAP);
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_WATCHPOINT),
              AMD_DBGAPI_EXCEPTION_WAVE_TRAP);
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_ASSERT_TRAP),
              AMD_DBGAPI_EXCEPTION_WAVE_TRAP);
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_TRAP),
              AMD_DBGAPI_EXCEPTION_WAVE_TRAP);
}

TEST(ProcessDbgapiStopReasonTest, SingleStep) {
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_SINGLE_STEP),
              AMD_DBGAPI_EXCEPTION_NONE);
}

TEST(ProcessDbgapiStopReasonTest, MathErrorReasons) {
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_FP_INPUT_DENORMAL),
              AMD_DBGAPI_EXCEPTION_WAVE_MATH_ERROR);
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_FP_DIVIDE_BY_0),
              AMD_DBGAPI_EXCEPTION_WAVE_MATH_ERROR);
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_FP_OVERFLOW),
              AMD_DBGAPI_EXCEPTION_WAVE_MATH_ERROR);
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_FP_UNDERFLOW),
              AMD_DBGAPI_EXCEPTION_WAVE_MATH_ERROR);
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_FP_INEXACT),
              AMD_DBGAPI_EXCEPTION_WAVE_MATH_ERROR);
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_FP_INVALID_OPERATION),
              AMD_DBGAPI_EXCEPTION_WAVE_MATH_ERROR);
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_INT_DIVIDE_BY_0),
              AMD_DBGAPI_EXCEPTION_WAVE_MATH_ERROR);
}

TEST(ProcessDbgapiStopReasonTest, MemoryViolation) {
    EXPECT_EQ(resume_exceptions_for_stop_reasons(AMD_DBGAPI_WAVE_STOP_REASON_MEMORY_VIOLATION),
              AMD_DBGAPI_EXCEPTION_WAVE_MEMORY_VIOLATION);
}
