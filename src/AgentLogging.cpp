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

#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

// Debug Agent Headers
#include "AgentLogging.h"
#include "AgentUtils.h"
#include "HSADebugAgentGDBInterface.h"

// The logging is disabled by default
// The logger can be enabled by env variables
// export ROCM_DEBUG_ENABLE_AGENTLOG='stdout'  --> will write to cout
// export ROCM_DEBUG_ENABLE_AGENTLOG='filename' --> will write to filename

static AgentLogManager* gs_pAgentLogManager;

static void AgentPrintLoadedDLL();
static bool WriteDLLPath(const std::string& dllName);

void AgentLogManager::LogVersionInfo()
{
    std::string infoStr = "";

    char agentName[] = {HSA_DEBUG_AGENT_VERSION};

    infoStr = std::string("ROCm debug agent version: ") + agentName + "\n";
    WriteLog(infoStr.c_str());
}

// Creates the SessionID_$Session_PID_$pid  string, will be reused for all agengLogging
void AgentLogManager::SetDebugSessionID(const char* pAgentLogPrefix,
                                        const char* pGdbSessionIDEnvVar)
{
    std::stringstream buffer;
    buffer.str("");
    if (pGdbSessionIDEnvVar == nullptr)
    {
        buffer << "PID_" << getpid();
    }
    else
    {
        buffer << "SessionID_" << pGdbSessionIDEnvVar << "_PID_" << getpid();
    }

    m_debugSessionID.assign(buffer.str());

    if (m_debugSessionID.size() > 64)
    {
        std::cout << "Code object file path exceeds max length." << "\n";
    }

    if (pAgentLogPrefix != nullptr)
    {
        m_AgentLogPrefix.assign(pAgentLogPrefix);
        buffer.str("");
        buffer << m_AgentLogPrefix << "_AgentLog_" << m_debugSessionID << ".log";
        m_AgentLogFileName.assign(buffer.str());
    }
}

void AgentLogManager::SetLoggingFromEnvVar()
{
    char* pLogNameEnvVar;
    pLogNameEnvVar = std::getenv("ROCM_DEBUG_ENABLE_AGENTLOG");

    // This is an internal variable set by the ROCm-GDB build script
    // Only add session id in file name when it is available
    char* pGDBSessionEnVar;
    pGDBSessionEnVar = std::getenv("ROCM_DEBUG_SESSION_ID");

    // Always set session id, it is used by tmp code object file naming
    SetDebugSessionID(pLogNameEnvVar, pGDBSessionEnVar);

    // We need both env variables
    if (pLogNameEnvVar != nullptr)
    {
        std::string opFileNamePrefix(pLogNameEnvVar);

        bool retCode = false;

        if (opFileNamePrefix == "stdout")
        {
            std::cout << "The AgentLog will print to stdout:\n";
            retCode = true;
        }
        else
        {
            retCode = OpenAgentLogFile();
        }

        if (retCode)
        {
            m_EnableLogging = true;
        }
    }
}

void AgentLogManager::WriteLog(const char* message)
{
    if (m_EnableLogging == false)
    {
        return;
    }

    if (!m_opStream.is_open())
    {
        WriteToStdOut(message);
    }
    else
    {
        WriteToOutStream(message);
    }
}

void AgentLogManager::WriteToOutStream(const char* message)
{
    m_opStream.flush();
    m_opStream << "AgentLOG> " << message;
    m_opStream.flush();
}

void AgentLogManager::WriteToStdOut(const char* message)const
{
    std::cout.flush();
    std::cout << "AgentLOG> " << message;
    std::cout.flush();
}

bool AgentLogManager::OpenAgentLogFile()
{
    if (!m_opStream.is_open())
    {

        m_opStream.open(m_AgentLogFileName.c_str(), std::ofstream::app);

        if (m_opStream.is_open())
        {
            m_opStream << "Start AgentLOG \n";
            std::cout << "The AgentLog File is: " << m_AgentLogFileName << "\n";
            std::cout.flush();
        }
    }
    return m_opStream.is_open();
}

void AgentLogManager::CloseLogFile()
{
    if (m_opStream.is_open())
    {
        m_opStream.close();

        std::cout << "Close the AgentLog File: " << m_AgentLogFileName << "\n";
        std::cout.flush();
    }
}

AgentLogManager::~AgentLogManager()
{
    if (m_opStream.is_open())
    {
        m_opStream.close();
    }
}

DebugAgentStatus AgentInitLogger()
{

    DebugAgentStatus status = DEBUG_AGENT_STATUS_FAILURE;

    gs_pAgentLogManager = new(std::nothrow)AgentLogManager;

    if (gs_pAgentLogManager != nullptr)
    {
        status = DEBUG_AGENT_STATUS_SUCCESS;
    }

    AgentPrintLoadedDLL();

    return status;
}

DebugAgentStatus AgentCloseLogger()
{
    DebugAgentStatus status = DEBUG_AGENT_STATUS_FAILURE;

    if (gs_pAgentLogManager != nullptr)
    {
        gs_pAgentLogManager->CloseLogFile();

        delete gs_pAgentLogManager;
        gs_pAgentLogManager = nullptr;
        status = DEBUG_AGENT_STATUS_SUCCESS;
    }

    return status;
}

// The message will add the endl always
void AgentLog(const char* message)
{
    if (gs_pAgentLogManager != nullptr)
    {
        gs_pAgentLogManager->WriteLog(message);
    }
}

// The message will add the endl always
void AgentOP(const char* message)
{
    std::cout.flush();
    std::cout << "[ROCm-gdb]: " << message;
    std::cout.flush();
}

// The message will add the endl always
void AgentPrint(const char* message)
{
    std::cout.flush();
    std::cout << "[rocr debug agent]: " << message;
    std::cout.flush();
}

// Write DLL to agent log
static bool WriteDLLPath(const std::string& dllName)
{
    std::string msg;
    bool ret = AgentWriteDLLPathToString(dllName, msg);
    msg += "\n";
    gs_pAgentLogManager->WriteLog(msg.c_str());
    return ret;
}

// Check whether relevant DLLs are loaded and print to stdout
static void AgentPrintLoadedDLL()
{
    WriteDLLPath("libhsa-runtime64.so.1");
    WriteDLLPath("libhsakmt.so.1");
}

// The message will add the endl always
void AgentErrorLog(const char* message)
{
#ifdef LOG_ERR_TO_STDERR
    std::cerr.flush();
    std::cerr << "Error: Debug Agent: " << message;
    std::cerr.flush();

    AgentLog(message);

#else
    AGENT_UNREFERENCED_PARAMETER(message);
#endif
}

void AgentWarningLog(const char* message)
{
#ifdef LOG_ERR_TO_STDERR
    std::cerr.flush();
    std::cerr << "Warning: Debug Agent: " << message;
    std::cerr.flush();

    AgentLog(message);

#else
    AGENT_UNREFERENCED_PARAMETER(message);
#endif
}

std::string AgentLogManager::GetDebugSessionID()
{
    return m_debugSessionID;
}

DebugAgentStatus AgentGetDebugSessionID(char* sessionID)
{
    if (gs_pAgentLogManager != nullptr)
    {
        std::strcpy(sessionID, gs_pAgentLogManager->GetDebugSessionID().c_str());
        return DEBUG_AGENT_STATUS_SUCCESS;
    }
    return DEBUG_AGENT_STATUS_FAILURE;
}
