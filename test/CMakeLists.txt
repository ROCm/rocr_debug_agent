################################################################################
##
## The University of Illinois/NCSA
## Open Source License (NCSA)
##
## Copyright (c) 2018, Advanced Micro Devices, Inc. All rights reserved.
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

#
#  Setup build environment
#
#  1) Defined ROCR-Runtime header and libraries path, ROCT-Thunk-Interface header and
#     libraries path, lightening compiler path, and OpenCL in Debug_Agent_PREFIX_PATH if they
#     are not in the default "/opt/rocm/" location.
#
#     export Debug_Agent_PREFIX_PATH=Path/to/ROC/Runtime/headers;Path/to/ROC/Runtime/libraries;
#                                    Path/to/Lightening/Compiler/Clang;
#                                    Path/to/OpenCL
#
#  2) Make a new folder called build under src folder
#
#     mkdir build
#
#  3) Enter into folder of build, and run CMAKE to generate makefile
#     and make it
#
#     cd build
#     cmake -DCMAKE_PREFIX_PATH=$Debug_Agent_PREFIX_PATH ..
#     make
#
#     @note: Add -DCMAKE_BUILD_TYPE=Debug if you want to build Debug
#

cmake_minimum_required(VERSION 3.5.0)

# Specify name of project to build, install and package
set(PROJECT_NAME "rocm-debug-agent-test")
set(TARGET_NAME "${PROJECT_NAME}" )
project(${PROJECT_NAME})

# Build is not supported on Windows plaform
if(WIN32)
  message(FATAL_ERROR "Windows platfom is not supported")
endif()

# Flag to enable / disable verbose output.
set(CMAKE_VERBOSE_MAKEFILE on)

# Compiler Preprocessor definitions.
add_definitions(-DUNIX_OS)
add_definitions(-DLINUX)
add_definitions(-DAMD_INTERNAL_BUILD)
add_definitions(-DLITTLEENDIAN_CPU=1)
add_definitions(-DHSA_LARGE_MODEL=)
add_definitions(-DHSA_DEPRECATED=)

# Enable debug trace
if(DEFINED ENV{CMAKE_DEBUG_TRACE})
  add_definitions(-DDEBUG_TRACE=1)
endif()

# Linux Compiler options
set(CMAKE_CXX_FLAGS "-std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

# Lightening Compiler options
set(LC_FLAGS -O0 -x cl -Xclang -cl-std=CL2.0 -g)

# CLANG options
if("$ENV{CXX}" STREQUAL "/usr/bin/clang++")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ferror-limit=1000000")
endif()

set(CMAKE_SKIP_BUILD_RPATH TRUE)

if(NOT DEFINED CMAKE_PREFIX_PATH AND DEFINED ENV{CMAKE_PREFIX_PATH})
  set(CMAKE_PREFIX_PATH $ENV{CMAKE_PREFIX_PATH})
endif()

# Extend Compiler flags based on Processor architecture
if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
  set(NBIT 64)
elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86")
  set(NBIT 32)
endif()

# Set project requirements
set(ROC_THUNK_NAME "hsakmt")
set(ROC_THUNK_LIBRARY "lib${ROC_THUNK_NAME}")
set(CORE_RUNTIME_NAME "hsa-runtime")
set(CORE_RUNTIME_TARGET "${CORE_RUNTIME_NAME}64")
set(CORE_RUNTIME_LIBRARY "lib${CORE_RUNTIME_TARGET}")

find_path(ROCR_INC hsa.h
  HINTS /opt/rocm/include/hsa)

find_library(ROCR_LIB ${CORE_RUNTIME_LIBRARY}.so
  HINTS /opt/rocm/lib)

find_library(HSAKMT_LIB ${ROC_THUNK_LIBRARY}.so
  HINTS /opt/rocm/lib)

find_path(LC_BIN clang
  HINTS /opt/rocm/opencl/bin/x86_64)

find_path(CL_BITL opencl.amdgcn.bc
  HINTS /opt/rocm/opencl/lib/x86_64/bitcode)

find_path(CL_INCP opencl-c.h
  HINTS /opt/rocm/opencl/include)

# Determine Roc Runtime header files are accessible
if(NOT ROCR_INC)
  message(FATAL_ERROR "ROC Runtime headers can't be found")
endif()

if(NOT ROCR_LIB)
  message(FATAL_ERROR "ERROR: ROC Runtime libraries can't be found")
endif()

if(NOT LC_BIN)
  message(FATAL_ERROR "Lightning Compiler can't be found")
endif()

if(NOT CL_BITL)
  message(FATAL_ERROR "Blit kernel can't be found")
endif()

if(NOT CL_INCP)
  message(FATAL_ERROR "OpenCL header can't be found")
endif()

if(NOT HSAKMT_LIB)
  message(FATAL_ERROR "ROC Thunk libraries can't be found")
endif()

# Blit kernel options
set(LC_BITL_FLAGS -Xclang -mlink-bitcode-file -Xclang ${CL_BITL}/opencl.amdgcn.bc -Xclang -mlink-bitcode-file -Xclang ${CL_BITL}/ockl.amdgcn.bc)

# Add cmake_modules to default module path
list ( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules" )
include(utils)

get_version("1.0.0")
message ( "-- LIB-VERSION: ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" )

# Bind the Major, Minor and Patch values
set(BUILD_VERSION_MAJOR ${VERSION_MAJOR})
set(BUILD_VERSION_MINOR ${VERSION_MINOR})
set(BUILD_VERSION_PATCH ${VERSION_PATCH})

# Basic Tool Chain Information
message(" ")
message("----------------NBIT: ${NBIT}")
message("------------Compiler: ${CMAKE_CXX_COMPILER}")
message("----Compiler-Version: ${CMAKE_CXX_COMPILER_VERSION}")
message("-----HSA-Runtime-Inc: ${ROCR_INC}")
message("-----HSA-Runtime-Lib: ${ROCR_LIB}")
message("-----------HSA-Thunk: ${HSAKMT_LIB}")
message("--Lightning-Compiler: ${LC_BIN}")
message("-----CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message("------------LC_FLAGS: ${LC_FLAGS}")
message("-------LC_BITL_FLAGS: ${LC_BITL_FLAGS}")
message("---CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(" ")

# Add directories to look for header files to compile
include_directories (${ROCR_INC})
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/Include)

# Add sources that belong to the project
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} Src)

# Compile LC code object
macro(LCStaticCompile ClSrc ObjPath TargetName)
  add_custom_command(
    OUTPUT ${ObjPath}
    DEPENDS "${ClSrc}"
    COMMAND ${LC_BIN}/clang ${LC_FLAGS} ${LC_BITL_FLAGS} -include ${CL_INCP}/opencl-c.h -target amdgcn--amdhsa -mcpu=${TargetName} -mcode-object-v3 ${ClSrc} -o ${ObjPath}
    COMMENT "Building LC code object ${ObjPath}..."
  )
endmacro()

# Test supported GPU ISAs
set(LC_TARGETS
  gfx900
  gfx906
  gfx908
)

# Test kernels
set(LC_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/vector_add_normal_kernel.cl"
  "${CMAKE_CURRENT_SOURCE_DIR}/vector_add_debug_trap_kernel.cl"
  "${CMAKE_CURRENT_SOURCE_DIR}/vector_add_memory_fault_kernel.cl"
)

# Build and generate header file for debug trap handler
foreach(LC_TARGET IN LISTS LC_TARGETS)
  foreach(LC_SRC IN LISTS LC_SRCS)
    get_filename_component(SRC_BASE_NAME "${LC_SRC}" NAME_WE)
    set(OUT_BASE_NAME ${SRC_BASE_NAME})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${LC_TARGET})
    set(OBJ_PATH ${LC_TARGET}/${OUT_BASE_NAME}.o)
    list ( APPEND OBJ_PATHS "${OBJ_PATH}" )
    LCStaticCompile(${LC_SRC} ${OBJ_PATH} ${LC_TARGET})
  endforeach()
endforeach()

# Add LC code object taget for dependency
add_custom_target(LC_OBJ DEPENDS ${OBJ_PATHS})

# Build and link the program
add_executable(${TARGET_NAME} ${Src} ${LC_SRCS})
add_dependencies(${TARGET_NAME} LC_OBJ)
target_link_libraries(${TARGET_NAME} PRIVATE ${ROCR_LIB} c stdc++ dl pthread rt)
# Horrible bodge to get to find a needed library.
#    The libhsa-runtime.so library has a DT_RUNPATH of
#    $ORIGIN:$ORIGIN/../../lib so the runtime linker can find
#    libhsakmt.so.1 However the compile time linker tries to open the
#    literal directories $ORIGIN and $ORIGIN/../../lib rather than
#    replacing $ORIGIN with the location of the libhsa-runtime.so
#    This is bug 20535
#    https://sourceware.org/bugzilla/show_bug.cgi?format=multiple&id=20535
#    bug 20535 is fixed in version 2.28 of binutils but Ubuntu 16.04 uses 2.26.1
target_link_libraries(${TARGET_NAME} PRIVATE ${HSAKMT_LIB})

# Add install directives for rocr-debug-agent-test
install(TARGETS ${TARGET_NAME} DESTINATION ${PROJECT_NAME} COMPONENT ${TARGET_NAME})

# Add packaging directives for rocm-debug-agent-test
set(CPACK_PACKAGE_NAME ${TARGET_NAME})
set(CPACK_PACKAGE_VENDOR "AMD")
set(CPACK_PACKAGE_VERSION_MAJOR ${BUILD_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${BUILD_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${BUILD_VERSION_PATCH})
set(CPACK_PACKAGE_CONTACT "Advanced Micro Devices Inc.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tests for rocm-debug-agent functions")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt" )

# RPM package specific variables
if(DEFINED CPACK_PACKAGING_INSTALL_PREFIX)
  set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "${CPACK_PACKAGING_INSTALL_PREFIX}")
endif()

include(CPack)

