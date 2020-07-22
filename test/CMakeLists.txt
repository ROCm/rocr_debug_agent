################################################################################
##
## The University of Illinois/NCSA
## Open Source License (NCSA)
##
## Copyright (c) 2018-2020, Advanced Micro Devices, Inc. All rights reserved.
##
## Permission is hereby granted, free of charge, to any person obtaining a copy
## of this software and associated documentation files (the "Software"), to
## deal with the Software without restriction, including without limitation
## the rights to use, copy, modify, merge, publish, distribute, sublicense,
## and/or sell copies of the Software, and to permit persons to whom the
## Software is furnished to do so, subject to the following conditions:
##
##  - Redistributions of source code must retain the above copyright notice,
##    this list of conditions and the following disclaimers.
##  - Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimers in
##    the documentation and/or other materials provided with the distribution.
##  - Neither the names of Advanced Micro Devices, Inc,
##    nor the names of its contributors may be used to endorse or promote
##    products derived from this Software without specific prior written
##    permission.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
## THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
## OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
## ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
## DEALINGS WITH THE SOFTWARE.
##
################################################################################

cmake_minimum_required(VERSION 3.8.0)

project(ROCM_DEBUG_AGENT_TEST VERSION ${ROCM_DEBUG_AGENT_VERSION})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "/opt/rocm/hip/cmake")
find_package(HIP REQUIRED MODULE)

file(GLOB SOURCES "*.cpp")

set_source_files_properties(${SOURCES} PROPERTIES HIP_SOURCE_PROPERTY_FORMAT 1)
hip_add_executable(rocm-debug-agent-test ${SOURCES}
  HIPCC_OPTIONS --amdgpu-target=gfx900 --amdgpu-target=gfx906
                --amdgpu-target=gfx908
  CLANG_OPTIONS -ggdb)

file(GLOB HEADERS "*.h")

install(FILES CMakeLists.txt run-test.py ${SOURCES} ${HEADERS}
  DESTINATION src/rocm-debug-agent
  COMPONENT tests)

enable_testing()
add_test(NAME rocm-debug-agent-test
  COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/run-test.py ${CMAKE_CURRENT_BINARY_DIR})
