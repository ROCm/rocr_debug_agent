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

#include <cassert>
#include <cstring>
#include <dlfcn.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <libelf.h>
#include <mutex>
#include <sys/stat.h>

// Debug Agent Headers
#include "AgentLogging.h"
#include "AgentUtils.h"
#include "HSADebugAgentGDBInterface.h"

// Just a simple function so that all the exit behavior can be handled in one place
// We can add logging parameters but it is expected that you would call the logging
// functions before you fatally exit
// We will try to restrict this function's usage so that the process  dies only from
// errors in system calls
void AgentFatalExit()
{
    AgentErrorLog("FatalExit\n");
    exit(-1);
}

std::string GetHsaStatusString(const hsa_status_t s)
{
    const char* pszbuff = { 0 };
    hsa_status_string(s, &pszbuff);

    std::string str = pszbuff;
    return str;
}

bool AgentIsFileExists(const char *fileName)
{
    std::ifstream infile(fileName);
    return infile.good();
}

bool AgentIsDirExists(const char *path)
{
    struct stat pathStat;
    if (stat(path, &pathStat))
    {
        return false;
    }
    if (!S_ISDIR(pathStat.st_mode))
    {
        return false;
    }
    return true;
}

DebugAgentStatus AgentCreateTmpDir()
{
    char* pTmpPathEnvVar = nullptr;

    if (g_gdbAttached)
    {
        return DEBUG_AGENT_STATUS_SUCCESS;
    }

    pTmpPathEnvVar = std::getenv("ROCM_DEBUG_SAVE_CODE_OBJECT");

    if (pTmpPathEnvVar != nullptr)
    {
        sprintf(g_codeObjDir, "/%s", pTmpPathEnvVar);
        g_deleteTmpFile = false;

        if ((mkdir(g_codeObjDir, 0777) != 0) && errno != EEXIST)
        {
            AGENT_WARNING("Failed creating temp code object file directory at given path, set path to default.");
            pTmpPathEnvVar = nullptr;
            memset(g_codeObjDir, 0, sizeof(g_codeObjDir));
        }
    }

    if (pTmpPathEnvVar == nullptr)
    {
        char sessionID[64];
        DebugAgentStatus agentStatus = AgentGetDebugSessionID(&sessionID[0]);
        if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
        {
            AGENT_ERROR("Interception: Cannot get debug session id");
            return agentStatus;
        }
        sprintf(g_codeObjDir, "/tmp/ROCm_Tmp_%s", sessionID);

        if (mkdir(g_codeObjDir, 0777) != 0)
        {
            AGENT_ERROR("Failed creating temp code object file directory at default path.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }
    }
    return DEBUG_AGENT_STATUS_SUCCESS;
}

DebugAgentStatus AgentDeleteFile(const char* ipFilename)
{
    DebugAgentStatus status = DEBUG_AGENT_STATUS_FAILURE;

    if (g_gdbAttached)
    {
        return DEBUG_AGENT_STATUS_SUCCESS;
    }

    if (ipFilename == nullptr)
    {
        AGENT_LOG("AgentDeleteFile: invalid filename");
        return status;
    }

    if (remove(ipFilename) != 0)
    {
        int err_no = errno;
        AGENT_ERROR("Error deleting " << ipFilename <<
                    ", errno: " << err_no << " " << strerror(err_no));
    }
    else
    {
        status = DEBUG_AGENT_STATUS_SUCCESS;
    }

    return status;
}

DebugAgentStatus AgentLoadFileAsSharedObject(const std::string& ipFilename)
{
    void* hModule = nullptr;
    DebugAgentStatus status = DEBUG_AGENT_STATUS_FAILURE;

    dlerror(); // clear error status before processing
    hModule = dlopen(ipFilename.c_str(), RTLD_LAZY );
    char* dllstatus = dlerror();

    if (nullptr != hModule)
    {
        AGENT_OP("File: "  << ipFilename << " loaded as a shared library");
        status = DEBUG_AGENT_STATUS_SUCCESS;
    }
    else
    {
        if (nullptr != dllstatus)
        {
            AGENT_ERROR("\"" <<ipFilename << "\"" << "Not Loaded (error: " << dllstatus << ")");
        }
        else
        {
            AGENT_ERROR(ipFilename  << "\t Not Loaded " << dllstatus );
        }
    }

    return status;
}

DebugAgentStatus AgentWriteBinaryToFile(const void*  pBinary, size_t binarySize, const char*  pFilename)
{
    DebugAgentStatus status = DEBUG_AGENT_STATUS_FAILURE;
    if (pBinary == nullptr)
    {
        AgentErrorLog("WriteBinaryToFile: Error Binary is null\n");
        return status;
    }

    if (binarySize <= 0)
    {
        AgentErrorLog("WriteBinaryToFile: Error Binary size is invalid\n");
        return status;
    }

    if (pFilename == nullptr)
    {
        AgentErrorLog("WriteBinaryToFile: Filename is nullptr\n");
        return status;
    }

    FILE* pFd = fopen(pFilename, "wb");

    if (pFd == nullptr)
    {
        AgentErrorLog("WriteBinaryToFile: Error opening file\n");
        assert(!"Error opening file\n");
        return status;
    }

    size_t retSize = fwrite(pBinary, sizeof(char), binarySize, pFd);

    fclose(pFd);

    if (retSize != binarySize)
    {
        AgentErrorLog("WriteBinaryToFile: Error writing to file\n");
        assert(!"WriteBinaryToFile: Error: fwrite failure.");
    }
    else
    {
        status = DEBUG_AGENT_STATUS_SUCCESS;
    }

    return status;
}

bool AgentWriteDLLPathToString(const std::string& dllName, std::string& msg)
{
    // Same as struct link_map in <link.h>
    typedef struct _LinkMap
    {
        void* pAddr;    // Difference between the address in the ELF file and the addresses in memory.
        char* pPath;    // Absolute file name object was found in.
        void* pLd;      // Dynamic section of the shared object.
        struct _LinkMap* pNext, *pPrev; // Chain of loaded objects.
    } LinkMap;

    bool ret = false;
    void* hModule = nullptr;
    char* status = nullptr;

    dlerror(); // clear error status before processing
    hModule = dlopen(dllName.c_str(), RTLD_LAZY | RTLD_NOLOAD);
    status = dlerror();

    if (nullptr != hModule)
    {
        // Get the path
        LinkMap* pLm = reinterpret_cast<LinkMap*>(hModule);
        msg.assign(pLm->pPath);
        msg += "\t Loaded";

        dlclose(hModule);
        ret = true;
    }
    else
    {
        if (nullptr != status)
        {
            msg += dllName + "\t Not Loaded (error " + status + ")";
        }
        else
        {
            msg += dllName + "\t Not Loaded (can be expected)";
        }

        ret = false;
    }

    return ret;
}

DebugAgentStatus SaveCodeObjectTempFile(uint64_t elfBaseAddress,
                                        uint64_t elfSize,
                                        CodeObjectInfo* pCodeObject)
{
    DebugAgentStatus agentStatus = DEBUG_AGENT_STATUS_SUCCESS;

    if (g_gdbAttached)
    {
        return DEBUG_AGENT_STATUS_SUCCESS;
    }

    // Check for code object directory, it is created in OnLoad.
    if (!AgentIsDirExists(g_codeObjDir))
    {
        AGENT_ERROR("Interception: The code object directory doesn't exist");
        return agentStatus;
    }

    agentStatus =  AgentWriteBinaryToFile((const void*)uintptr_t(elfBaseAddress), elfSize, pCodeObject->path);
    if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Cannot create temp code object file");
        return agentStatus;
    }

    return agentStatus;
}

DebugAgentStatus SaveWaveStateDumpToFile(std::stringstream& dump)
{
    DebugAgentStatus agentStatus = DEBUG_AGENT_STATUS_SUCCESS;

    // Check for code object directory, it is created in OnLoad.
    if (!AgentIsDirExists(g_codeObjDir))
    {
        AGENT_ERROR("Interception: The wave state directory doesn't exist");
        return agentStatus;
    }

    std::string waveStatePath = g_codeObjDir;
    waveStatePath += "/ROCm_Wave_State_Dump";
    std::ofstream opStream;
    opStream.open(waveStatePath, std::ofstream::app);

    if (!opStream.is_open())
    {
        AGENT_ERROR("Cannot open wave state dump file");
        return DEBUG_AGENT_STATUS_FAILURE;
    }

    opStream << dump.rdbuf();
    opStream.close();

    std::cout << "Wave States Dump File: " << waveStatePath << std::endl;
    return agentStatus;
}
