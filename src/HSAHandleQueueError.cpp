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

#include <iostream>

// HSA headers
#include <hsa_api_trace.h>

// Debug Agent Headers
#include "AgentLogging.h"
#include "AgentUtils.h"
#include "HSADebugAgentGDBInterface.h"
#include "HSADebugAgent.h"
#include "HSADebugInfo.h"
#include "HSAHandleQueueError.h"

static std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> FindWavesByQueue(QueueInfo* pQueue);
static void PrintQueueErrorInfo(hsa_status_t status, QueueInfo* pQueue);

void HSADebugAgentHandleQueueError(hsa_status_t status, hsa_queue_t* pHsaQueueT, void* pData)
{
    if (!g_debugAgentInitialSuccess)
    {
        return;
    }

    if (status == HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Queue status HSA_STATUS_SUCCESS when handle queue error.");
        return;
    }

    QueueInfo* pQueue = (QueueInfo*)pData;
    {
        std::lock_guard<std::mutex> lock(debugAgentAccessLock);

        GPUAgentInfo* pAgent = GetAgentByQueueID(pHsaQueueT->id);
        PreemptAgentQueues(pAgent);
        pQueue->queueStatus = status;

        if (g_gdbAttached)
        {
            // TODO qingchuan: add probe
        }
        else
        {
            std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> waves =
                FindWavesByQueue(pQueue);
            PrintQueueErrorInfo(status, pQueue);
            PrintWaves(pAgent, waves);
            allQueueWaves.clear();
        }

        // resume all quueues in HSA_Status_success
        ResumeAgentQueues(pAgent);
    }

    // Call the original callback registered when create queue in runtime
    if (allDebugAgentQueueInfo[pHsaQueueT->id].callback != nullptr)
    {
        auto* callback
            = reinterpret_cast<void (*)(hsa_status_t, hsa_queue_t*, void*)>
                    (allDebugAgentQueueInfo[pHsaQueueT->id].callback);

        (*callback)(status, pHsaQueueT, allDebugAgentQueueInfo[pHsaQueueT->id].data);
    }
}

static std::map<uint64_t, std::pair<uint64_t, WaveStateInfo *>> FindWavesByQueue(QueueInfo* pQueue)
{
    std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> waves;

    for (auto &wave : allQueueWaves[pQueue->queueId])
    {
        auto it = waves.find(wave.regs.pc);
        if (it != waves.end())
        {
            it->second.first++;
        }
        else
        {
            waves.insert(std::make_pair(wave.regs.pc,
                                        std::make_pair(1, &wave)));
        }
    }
    return waves;
}

static void PrintQueueErrorInfo(hsa_status_t status, QueueInfo* pQueue)
{
    GPUAgentInfo* pAgent = GetAgentByQueueID(pQueue->queueId);
    std::stringstream err;

    err << "\n";
    err << "Queue error state in GPU agent: " << pAgent->agentName << std::endl;
    err << "Node: " << pAgent->nodeId << std::endl;
    err << "Queue ID: " << pQueue->queueId << " (";

    switch (status)
    {
    case HSA_STATUS_ERROR_OUT_OF_RESOURCES:
        err << "Out of scratch;";
        break;
    case HSA_STATUS_ERROR_INCOMPATIBLE_ARGUMENTS:
        err << "Invalid dim;";
        break;
    case HSA_STATUS_ERROR_INVALID_ALLOCATION:
        err << "Invalid group memor;";
        break;
    case HSA_STATUS_ERROR_INVALID_CODE_OBJECT:
        err << "Invalid (or NULL) code;";
        break;
    case HSA_STATUS_ERROR_INVALID_PACKET_FORMAT:
        err << "Invalid format;";
        break;
    case HSA_STATUS_ERROR_INVALID_ARGUMENT:
        err << "Group is too large;";
        break;
    case HSA_STATUS_ERROR_INVALID_ISA:
        err << "Out of VGPRs;";
        break;
    case HSA_STATUS_ERROR_EXCEPTION:
        err << "Debug trap;";
        break;
    default: // HSA_STATUS_ERROR
        err << "Undefined code;";
    }
    err << ")\n\n";
    AGENT_PRINT(err.str());
}
