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

// Print general mempry fault info
static void PrintVMFaultInfo();

// Find the waves in XNACK error state
static std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> FindFaultyWaves();

hsa_status_t
HSADebugAgentHandleMemoryFault(hsa_amd_event_t event, void* pData)
{
    if (!g_debugAgentInitialSuccess)
    {
        return HSA_STATUS_ERROR;
    }

    if (event.event_type != HSA_AMD_GPU_MEMORY_FAULT_EVENT)
    {
        return HSA_STATUS_ERROR;
    }

    {
        std::lock_guard<std::mutex> lock(debugAgentAccessLock);

        DebugAgentStatus status = DEBUG_AGENT_STATUS_SUCCESS;
        GPUAgentInfo* pAgent = GetAgentFromList(reinterpret_cast<void*>(event.memory_fault.agent.handle));

        DebugAgentEventInfo *pEventInfo = _r_rocm_debug_info.pDebugAgentEvent;
        if (pEventInfo == nullptr)
        {
            AGENT_ERROR("Can not locate event info in _r_rocm_debug_info");
            return HSA_STATUS_ERROR;
        }

        // Update event info
        pEventInfo->eventType = DEBUG_AGENT_EVENT_MEMORY_FAULT;
        pEventInfo->eventData.eventMemoryFault.nodeId = pAgent->nodeId;
        pEventInfo->eventData.eventMemoryFault.virtualAddress = event.memory_fault.virtual_address;
        pEventInfo->eventData.eventMemoryFault.faultReasonMask = event.memory_fault.fault_reason_mask;


        if (g_gdbAttached)
        {
            // GDB breakpoint, it triggers GDB to probe wave state info.
            TriggerGPUEvent();
        }
        else
        {
            // TODO: Get all waves of all agents, force preempt the active ones.
            // Get all the waves for the faulty agent.
            QueueInfo* pQueue = pAgent->pQueueList;
            while (pQueue != nullptr)
            {
                CleanUpQueueWaveState(pAgent->nodeId, pQueue->queueId);
                status = ProcessQueueWaveStates(pAgent->nodeId, pQueue->queueId);
                if (status != DEBUG_AGENT_STATUS_SUCCESS)
                {
                    return HSA_STATUS_ERROR;
                }
                pQueue = pQueue->pNext;
            }

            // Print general mempry fault info.
            PrintVMFaultInfo();

            // Gather fault wave state info (vGPR, sGPR, LDS), and print
            std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> waves =
                FindFaultyWaves();
            PrintWaves(pAgent, waves);
        }
    }

    return HSA_STATUS_SUCCESS;
}

static std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> FindFaultyWaves()
{
    std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> faultyWaves;
    EventData memoryFaultInfo =_r_rocm_debug_info.pDebugAgentEvent->eventData;
    GPUAgentInfo *pAgent = GetAgentFromList(memoryFaultInfo.eventMemoryFault.nodeId);


    if (pAgent->agentStatus == AGENT_STATUS_UNSUPPORTED)
    {
        AGENT_ERROR("Due to unsupported agent ISA (supported ISA: gfx900/gfx906), can not print waves in Agent: "
                    << pAgent->agentName);
        return faultyWaves;
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
    return faultyWaves;
}

static void PrintVMFaultInfo()
{
    if (_r_rocm_debug_info.pDebugAgentEvent == nullptr)
    {
        AGENT_ERROR("Can not find memory fault info when print");
        return;
    }

    if (_r_rocm_debug_info.pDebugAgentEvent->eventType != DEBUG_AGENT_EVENT_MEMORY_FAULT)
    {
        AGENT_ERROR("Wrong event type when print memory fault info");
        return;
    }

    EventData memoryFaultInfo =_r_rocm_debug_info.pDebugAgentEvent->eventData;
    std::stringstream err;

    uint64_t fault_page_idx = memoryFaultInfo.eventMemoryFault.virtualAddress >> 0xC;

    err << "\n";
    err << "Memory access fault at GPU Node: " << memoryFaultInfo.eventMemoryFault.nodeId <<std::endl;
    err << "Address: 0x" << std::hex << std::uppercase << fault_page_idx << "xxx (";

    if ((memoryFaultInfo.eventMemoryFault.faultReasonMask & HSA_AMD_MEMORY_FAULT_PAGE_NOT_PRESENT) > 0)
    {
        err << "page not present;";
    }
    if ((memoryFaultInfo.eventMemoryFault.faultReasonMask & HSA_AMD_MEMORY_FAULT_READ_ONLY) > 0)
    {
        err << "write access to a read-only page;";
    }
    if ((memoryFaultInfo.eventMemoryFault.faultReasonMask & HSA_AMD_MEMORY_FAULT_NX) > 0)
    {
        err << "execute access to a non-executable page;";
    }
    if ((memoryFaultInfo.eventMemoryFault.faultReasonMask & HSA_AMD_MEMORY_FAULT_HOST_ONLY) > 0)
    {
        err << "access to host access only;";
    }
    if ((memoryFaultInfo.eventMemoryFault.faultReasonMask & HSA_AMD_MEMORY_FAULT_DRAM_ECC) > 0)
    {
        err << "uncorrectable DRAM ECC failure;";
    }
    if ((memoryFaultInfo.eventMemoryFault.faultReasonMask & HSA_AMD_MEMORY_FAULT_IMPRECISE) > 0)
    {
        err << "can't determine the exact fault address;";
    }
    if ((memoryFaultInfo.eventMemoryFault.faultReasonMask & HSA_AMD_MEMORY_FAULT_SRAM_ECC) > 0)
    {
        err << "SRAM ECC failure;";
    }
    if ((memoryFaultInfo.eventMemoryFault.faultReasonMask & HSA_AMD_MEMORY_FAULT_HANG) > 0)
    {
        err << "GPU reset following unspecified hang;";
    }
    err << ")\n\n";
    AGENT_PRINT(err.str());
}
