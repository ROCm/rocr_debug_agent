////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2018, Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

#include <hsa_api_trace.h>

// Debug Agent Headers
#include "HSADebugAgent.h"
#include "HSADebugAgentGDBInterface.h"
#include "HSADebugInfo.h"
#include "HSAHandleDebugTrapSignal.h"

// Debug Agent Probes. To skip dependence upon semaphore variables,
// include "<sys/sdt.h>" first.
#include <sys/sdt.h>
#include "HSADebugAgentGDBProbes.h"

#include <sys/types.h>

bool HSADebugTrapSignalHandler(hsa_signal_value_t signalValue, void* arg)
{
    if (!g_debugAgentInitialSuccess)
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(debugAgentAccessLock);

        // Clear the signal
        hsa_signal_store_relaxed(debugTrapSignal, 0);

        ROCM_GDB_AGENT_GPU_USER_BREAKPOINT();
    }

    return true;
}
