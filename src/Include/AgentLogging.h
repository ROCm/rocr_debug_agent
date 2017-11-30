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

#ifndef AGENT_LOGGING_H_
#define AGENT_LOGGING_H_

#include <sstream>
#include <fstream>

// HSA Headers
#include <hsa.h>

// Debug Agent Headers
#include "HSADebugAgent.h"

#if defined (_WIN32)
    #define AGENT_UNREFERENCED_PARAMETER( x ) ( x )
#elif defined (__linux__) || defined (__CYGWIN__)
    #define AGENT_UNREFERENCED_PARAMETER( x )
#endif

// Always log errors
#define LOG_ERR_TO_STDERR 1

// Macro to create a stringstream, initialize it and use existing AgentLog()
#define AGENT_LOG(stream)           \
{                                   \
    std::stringstream buffer;       \
    buffer.str("");                 \
    buffer << stream << "\n";       \
    AgentLog(buffer.str().c_str()); \
}

// Macro to create a stringstream, initialize it and use existing AgentErrorLog()
#define AGENT_ERROR(stream)                 \
{                                           \
    std::stringstream buffer;               \
    buffer.str("");                         \
    buffer << stream << "\n";               \
    AgentErrorLog(buffer.str().c_str());    \
}

// Macro to create a stringstream, initialize it and use existing AgentWarningLog()
#define AGENT_WARNING(stream)               \
{                                           \
    std::stringstream buffer;               \
    buffer.str("");                         \
    buffer << stream << "\n";               \
    AgentWarningLog(buffer.str().c_str());  \
}

// Macro to create a stringstream, initialize it and use existing AgentOp()
#define AGENT_OP(stream)            \
{                                   \
    std::stringstream buffer;       \
    buffer.str("");                 \
    buffer << stream << "\n";       \
    AgentOP(buffer.str().c_str());  \
}

// Macro to create a stringstream, initialize it and use existing AgentPrint()
#define AGENT_PRINT(stream)            \
{                                      \
    std::stringstream buffer;          \
    buffer.str("");                    \
    buffer << stream << "\n";          \
    AgentPrint(buffer.str().c_str());  \
}

/// This logger is not threadsafe
class AgentLogManager
{
private:
    std::string m_AgentLogFileName;

    std::ofstream m_opStream;

    std::string m_AgentLogPrefix;

    std::string m_debugSessionID;

    /// Disable copy constructor
    AgentLogManager(const AgentLogManager&);

    /// Disable assignment operator
    AgentLogManager& operator=(const AgentLogManager&);

    void LogVersionInfo();

    void SetLoggingFromEnvVar();

    // These logging functions take a message only as an input.
    // The functions simply add some prefix and print the message.
    // The cout flush functions are only helpful to try and dump output asap
    void WriteToStdOut(const char* message) const;

    void WriteToOutStream(const char* message) ;

    void SetDebugSessionID(const char* pAgentLogPrefix,
                           const char* pGdbSessionIDEnvVar);

public:
    bool m_EnableLogging ;

    bool m_EnableISADump;

    AgentLogManager():
        m_AgentLogFileName(""),
        m_opStream(),
        m_AgentLogPrefix(""),
        m_debugSessionID(""),
        m_EnableLogging(false),
        m_EnableISADump(false)
    {
        SetLoggingFromEnvVar();

        LogVersionInfo();
    }

    ~AgentLogManager();

    bool OpenAgentLogFile();

    void CloseLogFile();

    void WriteLog(const char* message);

    std::string GetDebugSessionID();
};

DebugAgentStatus AgentInitLogger();

DebugAgentStatus AgentCloseLogger();

void AgentLog(const char*);

void AgentOP(const char*);

void AgentPrint(const char*);

void AgentErrorLog(const char*);

void AgentWarningLog(const char*);

DebugAgentStatus AgentGetDebugSessionID(char* sessionID);

#endif // AGENT_LOGGING_H_
