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

#ifndef AGENT_UTILS_H_
#define AGENT_UTILS_H_

#include <vector>

// HSA Headers
#include <hsa.h>

// Debug Agent Headers
#include "HSADebugAgent.h"

// forward declaration
typedef struct _CodeObjectInfo CodeObjectInfo;

// A fatal exit function so that all the exit behavior can be handled in one place
void AgentFatalExit();

// Get symbols from an ELF binary
void ExtractSymbolListFromELFBinary(const void* pBinary,
                                    size_t binarySize,
                                    std::vector<std::pair<std::string, uint64_t>>& outputSymbols);

std::string GetHsaStatusString(const hsa_status_t s);

DebugAgentStatus AgentLoadFileAsSharedObject(const std::string& ipFilename);

bool AgentIsFileExists(const char* fileName);

bool AgentIsDirExists(const char* path);

// The temp code object files are deleted by default
// export ROCM_DEBUG_SAVE_CODE_OBJECT='path' --> will save all code object files to path
DebugAgentStatus AgentCreateTmpDir();

/// Delete a file
DebugAgentStatus AgentDeleteFile(const char* ipFilename);

/// Write a binary buffer to file
DebugAgentStatus AgentWriteBinaryToFile(const void* pBinary, const size_t binarySize, const char* pFilename);

bool AgentWriteDLLPathToString(const std::string& dllName, std::string& msg);

// Save code object files to temp directory.
DebugAgentStatus SaveCodeObjectTempFile(uint64_t elfBaseAddress,
                                        uint64_t elfSize,
                                        CodeObjectInfo* pCodeObject);

// Save wave state dump file to temp directory.
DebugAgentStatus SaveWaveStateDumpToFile(std::stringstream& dump);

// Generally useful utility functions

// Calculates the floor value aligned based on parameter of alignment.
template <typename T>
T AlignDown(T value, size_t alignment) {
  return (T)(value & ~(alignment - 1));
}

template <typename T>
T* AlignDown(T* value, size_t alignment) {
  return (T*)AlignDown((intptr_t)value, alignment);
}

// Calculates the ceiling value aligned based on parameter of
template <typename T>
T AlignUp(T value, size_t alignment) {
  return AlignDown((T)(value + alignment - 1), alignment);
}

template <typename T>
T* AlignUp(T* value, size_t alignment) {
  return (T*)AlignDown((intptr_t)((uint8_t*)value + alignment - 1), alignment);
}

// Checks if the input value is at the boundary of alignment, if it is,
template <typename T>
bool IsMultipleOf(T* value, size_t alignment) {
  return (AlignUp(value, alignment) == value);
}

template <typename T>
bool IsMultipleOf(T value, size_t alignment) {
  return (AlignUp(value, alignment) == value);
}

#endif // AGENT_UTILS_H
