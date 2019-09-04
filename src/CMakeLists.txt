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
#     libraries path, and lightening compiler path in Debug_Agent_PREFIX_PATH if they
#     are not in the default "/opt/rocm/" location.
#
#     export Debug_Agent_PREFIX_PATH=Path/to/ROC/Runtime/headers;Path/to/ROC/Runtime/libraries;
#                                    Path/to/ROC/Thunk/headers;Path/to/ROC/Thunk/libraries;
#                                    Path/to/Lightening/Compiler/Clang
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

project(rocr_debug_agent VERSION 1.0.0)
set(TARGET_NAME "${PROJECT_NAME}64" )
set(BUILD_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(BUILD_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(BUILD_VERSION_PATCH ${PROJECT_VERSION_PATCH})

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

# CLANG options
if("$ENV{CXX}" STREQUAL "/usr/bin/clang++")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ferror-limit=1000000")
endif()

set(CMAKE_SHARED_LINKER_FLAGS "-shared -Wl,-Bdynamic -Wl,-Bsymbolic-functions")
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

# Check if dtrace is installed
find_program(DTRACE dtrace)

# Check if sys/sdt.h is avaialable
include(CheckIncludeFile)
check_include_file("sys/sdt.h" HAVE_SYS_SDT_H)
if(NOT DTRACE OR NOT HAVE_SYS_SDT_H)
  message(FATAL_ERROR "SystemTap not found, "
      "please install systemtap-sdt-dev (debian), "
      "or systemtap-sdt-devel (rpm).")
endif()

# Set project requirements
set(ROC_THUNK_NAME "hsakmt")
set(CORE_RUNTIME_NAME "hsa-runtime")
set(ROC_THUNK_LIBRARY "lib${ROC_THUNK_NAME}")
set(CORE_RUNTIME_TARGET "${CORE_RUNTIME_NAME}64")
set(CORE_RUNTIME_LIBRARY "lib${CORE_RUNTIME_TARGET}")

find_path(ROCR_INC hsa.h
  HINTS /opt/rocm/include/hsa)

find_library(ROCR_LIB ${CORE_RUNTIME_LIBRARY}.so
  HINTS /opt/rocm/lib)

find_path(HSAKMT_INC hsakmt.h
  HINTS /opt/rocm/include)

find_library(HSAKMT_LIB ${ROC_THUNK_LIBRARY}.so
  HINTS /opt/rocm/lib)

find_path(LC_BIN clang
  HINTS /opt/rocm/opencl/bin/x86_64)

# Determine Roc Runtime header files are accessible
if(NOT ROCR_INC)
  message(FATAL_ERROR "ROC Runtime headers can't be found")
endif()

if(NOT ROCR_LIB)
  message(FATAL_ERROR "ERROR: ROC Runtime libraries can't be found")
endif()

if(NOT HSAKMT_INC)
  message(FATAL_ERROR "ROC Thunk headers can't be found")
endif()

if(NOT HSAKMT_LIB)
  message(FATAL_ERROR "ROC Thunk libraries can't be found")
endif()

if(NOT LC_BIN)
  message(FATAL_ERROR "Lightning Compiler can't be found")
endif()

# Add cmake_modules to default module path
list ( APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules" )
include(utils)

# Basic Tool Chain Information
message(" ")
message("----------------NBIT: ${NBIT}")
message("------------Compiler: ${CMAKE_CXX_COMPILER}")
message("----Compiler-Version: ${CMAKE_CXX_COMPILER_VERSION}")
message("-----HSA-Runtime-Inc: ${ROCR_INC}")
message("-----HSA-Runtime-Lib: ${ROCR_LIB}")
message("-------HSA-Thunk-Inc: ${HSAKMT_INC}")
message("-----------HSA-Thunk: ${HSAKMT_LIB}")
message("--Lightning-Compiler: ${LC_BIN}")
message("-----CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message("---CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(" ")

# Add directories to look for header files to compile
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories (${HSAKMT_INC})
include_directories (${ROCR_INC})
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/Include)

# Add sources that belong to the project
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} Src)

macro(LLVMAssemble AsmPath ObjPath TargetName ISAVersionMinor ISAVersionStepping)
  add_custom_command(
    OUTPUT ${ObjPath}
    DEPENDS "${AsmPath}"
    COMMAND ${LC_BIN}/clang -x assembler -Wa,-defsym,.LISA_VERSION_MINOR=${ISAVersionMinor} -Wa,-defsym,.LISA_VERSION_STEPPING=${ISAVersionStepping} -target amdgcn--amdhsa -mcpu=${TargetName} -mno-code-object-v3 -c -o ${ObjPath} "${AsmPath}"
    COMMENT "Building LLVM assembly object ${ObjPath}..."
  )
endmacro()

macro(LLVMLink ObjPath CodeObjPath)
  add_custom_command(
    OUTPUT ${CodeObjPath}
    DEPENDS ${ObjPath}
    COMMAND ${LC_BIN}/clang -target amdgcn--amdhsa -o ${CodeObjPath} ${ObjPath}
    COMMENT "Linking LLVM code object ${CodeObjPath}..."
  )
endmacro()

macro(LLVMStringify CodeObjPath HdrPath TargetName)
  add_custom_command(
    OUTPUT ${HdrPath}
    DEPENDS ${CodeObjPath}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/create_debug_trap_co_hex_file.sh ${CodeObjPath} ${HdrPath} ${TargetName}
    COMMENT "Generating LLVM code object header ${HdrPath}..."
  )
endmacro()

set(TARGET_ISAS
  "gfx900"
  "gfx906"
  "gfx908"
)

macro(DTraceGenerateHeader ScriptPath HdrPath)
  add_custom_command(
    OUTPUT ${HdrPath}
    DEPENDS ${ScriptPath}
    COMMAND ${DTRACE} -h -s ${ScriptPath} -o ${HdrPath}
    COMMENT "Generating SystemTap probes header ${HdrPath}..."
  )
endmacro()

set(DTRACE_SCRIPTS
  "${CMAKE_CURRENT_SOURCE_DIR}/HSADebugAgentGDBProbes.d"
)

# Build and generate header file for debug trap handler
set(ASM_HDR_PATHS "")
set(ASM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/HSATrapHandler_s_gfx9xx.s")
foreach(ISA IN LISTS TARGET_ISAS)
  set(OUT_BASE_NAME "HSATrapHandler_s_${ISA}")
  set(OBJ_PATH ${OUT_BASE_NAME}.o)
  set(CODE_OBJ_PATH ${OUT_BASE_NAME}.co)
  set(HDR_PATH ${CMAKE_CURRENT_BINARY_DIR}/${OUT_BASE_NAME}.h)
  STRING(REGEX MATCHALL "([0-9])" ISA_VERSION "${ISA}")
  list(GET ISA_VERSION 1 ISA_VERSION_MINOR)
  list(GET ISA_VERSION 2 ISA_VERSION_STEPPING)

  LLVMAssemble("${ASM_SRC}" ${OBJ_PATH} ${ISA} ${ISA_VERSION_MINOR} ${ISA_VERSION_STEPPING})
  LLVMLink(${OBJ_PATH} ${CODE_OBJ_PATH})
  LLVMStringify(${CODE_OBJ_PATH} ${HDR_PATH} ${ISA})
  list(APPEND ASM_HDR_PATHS ${HDR_PATH})
endforeach()

# Generate the header files for the GDB probes
set(PROBES_HDR_PATHS "")
foreach(DTRACE_SCRIPT IN LISTS DTRACE_SCRIPTS)
  get_filename_component(SRC_BASE_NAME "${DTRACE_SCRIPT}" NAME_WE)
  set(OUT_BASE_NAME ${SRC_BASE_NAME})
  set(HDR_PATH ${CMAKE_CURRENT_BINARY_DIR}/${OUT_BASE_NAME}.h)
  DTraceGenerateHeader(${DTRACE_SCRIPT} ${HDR_PATH})
  list(APPEND PROBES_HDR_PATHS ${HDR_PATH})
endforeach()

# Build and link the program
add_library(${TARGET_NAME} SHARED ${Src} ${ASM_HDR_PATHS} ${PROBES_HDR_PATHS})
target_link_libraries(${TARGET_NAME} PRIVATE ${ROCR_LIB} ${HSAKMT_LIB} c stdc++ dl pthread rt)

set_target_properties(rocr_debug_agent64 PROPERTIES
  CXX_STANDARD 11
  CXX_STANDARD_REQUIRED Yes
  CXX_EXTENSIONS No
  VERSION "${BUILD_VERSION_MAJOR}.${BUILD_VERSION_MINOR}"
  SOVERSION "${BUILD_VERSION_MAJOR}")

# Add install directives for rocr_debug_agent
install(TARGETS ${TARGET_NAME} DESTINATION lib)

# Add packaging directives for rocr_debug_agent
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "AMD")
set(CPACK_PACKAGE_VERSION_MAJOR ${BUILD_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${BUILD_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${BUILD_VERSION_PATCH})
set(CPACK_PACKAGE_CONTACT "Advanced Micro Devices Inc.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Radeon Open Compute (ROCm) Runtime debug agent")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt" )

# Debian package specific variables
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/ROCm-Developer-Tools/rocr_debug_agent")

# RPM package specific variables
if(DEFINED CPACK_PACKAGING_INSTALL_PREFIX)
  set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "${CPACK_PACKAGING_INSTALL_PREFIX}")
endif()

include(CPack)

