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

#ifndef HSA_DEBUG_AGENT_H_
#define HSA_DEBUG_AGENT_H_

#include <mutex>
#include <atomic>

// forward declaration.
typedef struct _RocmGpuDebug RocmGpuDebug;

typedef enum
{
    DEBUG_AGENT_STATUS_SUCCESS = 0,  // A success
    DEBUG_AGENT_STATUS_FAILURE       // A failure in the debug agent
} DebugAgentStatus;

// This is the instance of the structure probed by the GDB.
extern "C" RocmGpuDebug _r_rocm_debug_info;

// Temp direcoty path for code object files
extern char g_codeObjDir[92];

// whether delete tmp code object files
extern bool g_deleteTmpFile;

// Debug agent initialization status
extern std::atomic<bool> g_debugAgentInitialSuccess;

// ISA name of gfx900
const char gfx900[] = "amdgcn-amd-amdhsa--gfx900";

// ISA name of gfx906
const char gfx906[] = "amdgcn-amd-amdhsa--gfx906";

// ISA name of gfx908
const char gfx908[] = "amdgcn-amd-amdhsa--gfx908";

// Agent name of gfx900
const char gfx900AgentName[] = "AMD gfx900";

// Agent name of gfx906
const char gfx906AgentName[] = "AMD gfx906";

// Agent name of gfx908
const char gfx908AgentName[] = "AMD gfx908";

// lock for access debug agenet
extern std::mutex debugAgentAccessLock;

extern "C" bool OnLoad(void *pTable,
                       uint64_t runtimeVersion, uint64_t failedToolCount,
                       const char *const *pFailedToolNames);

extern "C" void OnUnload();

#endif // HSA_DEBUG_AGENT_H
