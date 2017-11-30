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
#include "HSAHandleMemoryFault.h"

// Update fault info of the fautly agent
static void UpdateMemoryFaultInfo(hsa_amd_gpu_memory_fault_info_t* fault);

// Print general mempry fault info
static void PrintVMFaultInfo(const GPUAgentInfo* pFaultyAgent);

// Find the waves in XNACK error state
static std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> FindFaultyWaves();

hsa_status_t
HSADebugAgentHandleMemoryFault(hsa_amd_event_t event, void* pData)
{
    if (!g_debugAgentInitialSuccess)
    {
        return HSA_STATUS_ERROR;
    }

    if (event.event_type != GPU_MEMORY_FAULT_EVENT)
    {
        return HSA_STATUS_ERROR;
    }

    debugInfoLock.lock();
    DebugAgentStatus status = DEBUG_AGENT_STATUS_SUCCESS;
    hsa_amd_gpu_memory_fault_info_t fault = event.memory_fault;

    UpdateMemoryFaultInfo(&fault);

    // TODO: Get all waves of all agents, force preempt the active ones.
    // Get all the waves for the faulty agent.
    GPUAgentInfo* pAgent = GetAgentFromList(fault.agent);
    QueueInfo* pQueue = pAgent->pQueueList;
    while (pQueue != nullptr)
    {
        CleanUpQueueWaveState(pAgent->nodeId, pQueue->queueId);
        status = ProcessQueueWaveStates(pAgent->nodeId, pQueue->queueId);
        if (status != DEBUG_AGENT_STATUS_SUCCESS)
        {
            debugInfoLock.unlock();
            return HSA_STATUS_ERROR;
        }
        pQueue = pQueue->pNext;
    }

    if (g_gdbAttached)
    {
        debugInfoLock.unlock();
        // GDB breakpoint, it triggers GDB to probe wave state info.
        TriggerGPUEventFault();
        std::abort();
    }

    // Print general mempry fault info.
    PrintVMFaultInfo(pAgent);

    // Gather fault wave state info (vGPR, sGPR, LDS), and print
    std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> waves =
        FindFaultyWaves();
    PrintWaves(waves);
    debugInfoLock.unlock();

    // TODO: not abort() if runtime needs to take over.
    std::abort();

    if (status == DEBUG_AGENT_STATUS_SUCCESS)
    {
        return HSA_STATUS_SUCCESS;
    }
    else
    {
        return HSA_STATUS_ERROR;
    }
}

static void UpdateMemoryFaultInfo(hsa_amd_gpu_memory_fault_info_t* pFault)
{
    GPUAgentInfo* pAgent = GetAgentFromList(pFault->agent);
    if (pAgent->agentStatus == AGENT_STATUS_ACTIVE)
    {
        pAgent->agentStatus = AGENT_STATUS_MEMORY_FAULT;
        pAgent->memoryFaultInfo.virtualAddress = pFault->virtual_address;
        pAgent->memoryFaultInfo.faultReasonMask = pFault->fault_reason_mask;
    }
}

static std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> FindFaultyWaves()
{
    GPUAgentInfo* pAgent = _r_amd_gpu_debug.pAgentList;
    std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> faultyWaves;
    while(pAgent != nullptr)
    {
        if (pAgent->agentStatus == AGENT_STATUS_UNSUPPORTED)
        {
            AGENT_ERROR("Due to unsupported agent ISA (supported ISA: gfx900), can not print waves in Agent: "
                        << pAgent->agentName);
            pAgent = pAgent->pNext;
            continue;
        }
        else if (pAgent->agentStatus == AGENT_STATUS_ACTIVE)
        {
            pAgent = pAgent->pNext;
            continue;
        }

        QueueInfo* pQueue = pAgent->pQueueList;
        while (pQueue != nullptr)
        {
            WaveStateInfo* pWave = pQueue->pWaveList;
            while (pWave != nullptr)
            {
                if (SQ_WAVE_TRAPSTS_XNACK_ERROR(pWave->regs.trapsts))
                {
                    pWave->regs.pc += 0x8;
                    pQueue->queueStatus = QUEUE_STATUS_FAILURE;

                    // Update the faulty waves for printing.
                    std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>>::iterator it;
                    it = faultyWaves.find(pWave->regs.pc);
                    if (it != faultyWaves.end())
                    {
                        it->second.first ++;
                    }
                    else
                    {
                        faultyWaves.insert(std::make_pair(pWave->regs.pc,
                                                          std::make_pair(1, pWave)));
                    }

                }
                pWave = pWave->pNext;
            }
            pQueue = pQueue->pNext;
        }
        pAgent = pAgent->pNext;
    }
    return faultyWaves;
}

static void PrintVMFaultInfo(const GPUAgentInfo* pFaultyAgent)
{
    MemoryFaultInfo memoryFaultInfo = pFaultyAgent->memoryFaultInfo;
    std::stringstream err;

    uint64_t fault_page_idx = memoryFaultInfo.virtualAddress >> 0xC;

    err << "\n";
    err << "Memory access fault by GPU agent: " << pFaultyAgent->agentName <<std::endl;
    err << "Node: " << pFaultyAgent->nodeId <<std::endl;
    err << "Address: 0x" << std::hex << std::uppercase << fault_page_idx << "xxx (";

    if ((memoryFaultInfo.faultReasonMask & 0x00000001) > 0)
    {
        err << "page not present;";
    }
    if ((memoryFaultInfo.faultReasonMask & 0x00000010) > 0)
    {
        err << "write access to a read-only page;";
    }
    if ((memoryFaultInfo.faultReasonMask & 0x00000100) > 0)
    {
        err << "execute access to a non-executable page;";
    }
    if ((memoryFaultInfo.faultReasonMask & 0x00001000) > 0)
    {
        err << "access to host access only;";
    }
    if ((memoryFaultInfo.faultReasonMask & 0x00010000) > 0)
    {
        err << "uncorrectable ECC failure;";
    }
    if ((memoryFaultInfo.faultReasonMask & 0x00100000) > 0)
    {
        err << "can't determine the exact fault address;";
    }
    err << ")\n\n";
    AGENT_PRINT(err.str());
}
