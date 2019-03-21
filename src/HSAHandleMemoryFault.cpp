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
static void PrintVMFaultInfo(uint32_t nodeId, hsa_amd_event_t event);

// Find the waves in XNACK error state
static std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> FindFaultyWaves(GPUAgentInfo *pAgent);

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

        if (g_gdbAttached)
        {
            // TODO qingchuan: Add Probe for GDB.
        }
        else
        {
            // TODO: Get all waves of all agents, force preempt the active ones.
            // Get all the waves for the faulty agent.
            QueueInfo* pQueue = pAgent->pQueueList;
            while (pQueue != nullptr)
            {
                status = ProcessQueueWaveStates(pAgent->nodeId, pQueue->queueId);
                if (status != DEBUG_AGENT_STATUS_SUCCESS)
                {
                    return HSA_STATUS_ERROR;
                }
                pQueue = pQueue->pNext;
            }

            // Print general mempry fault info.
            PrintVMFaultInfo(pAgent->nodeId, event);

            // Gather fault wave state info (vGPR, sGPR, LDS), and print
            std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> waves =
                FindFaultyWaves(pAgent);
            PrintWaves(pAgent, waves);
            allQueueWaves.clear();
        }
    }

    return HSA_STATUS_SUCCESS;
}

static std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> FindFaultyWaves(GPUAgentInfo *pAgent)
{
    std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> faultyWaves;

    if (pAgent->agentStatus == AGENT_STATUS_UNSUPPORTED)
    {
        AGENT_ERROR("Due to unsupported agent ISA (supported ISA: gfx900/gfx906), can not print waves in Agent: "
                    << pAgent->agentName);
        return faultyWaves;
    }

    for (auto &queueWaves : allQueueWaves)
    {
        for (auto &wave : queueWaves.second)
        {
            if (SQ_WAVE_TRAPSTS_XNACK_ERROR(wave.regs.trapsts))
            {
                wave.regs.pc += 0x8;

                // Update the faulty waves for printing.
                auto it = faultyWaves.find(wave.regs.pc);
                if (it != faultyWaves.end())
                {
                    it->second.first ++;
                }
                else
                {
                    faultyWaves.insert(std::make_pair(wave.regs.pc,
                                                        std::make_pair(1, &wave)));
                }

            }
        }
    }
    return faultyWaves;
}

static void PrintVMFaultInfo(uint32_t nodeId, hsa_amd_event_t event)
{
    std::stringstream err;

    uint64_t fault_page_idx = event.memory_fault.virtual_address >> 0xC;

    err << "\n";
    err << "Memory access fault at GPU Node: " << nodeId <<std::endl;
    err << "Address: 0x" << std::hex << std::uppercase << fault_page_idx << "xxx (";

    if ((event.memory_fault.fault_reason_mask & HSA_AMD_MEMORY_FAULT_PAGE_NOT_PRESENT) > 0)
    {
        err << "page not present;";
    }
    if ((event.memory_fault.fault_reason_mask & HSA_AMD_MEMORY_FAULT_READ_ONLY) > 0)
    {
        err << "write access to a read-only page;";
    }
    if ((event.memory_fault.fault_reason_mask & HSA_AMD_MEMORY_FAULT_NX) > 0)
    {
        err << "execute access to a non-executable page;";
    }
    if ((event.memory_fault.fault_reason_mask & HSA_AMD_MEMORY_FAULT_HOST_ONLY) > 0)
    {
        err << "access to host access only;";
    }
    if ((event.memory_fault.fault_reason_mask & HSA_AMD_MEMORY_FAULT_DRAM_ECC) > 0)
    {
        err << "uncorrectable DRAM ECC failure;";
    }
    if ((event.memory_fault.fault_reason_mask & HSA_AMD_MEMORY_FAULT_IMPRECISE) > 0)
    {
        err << "can't determine the exact fault address;";
    }
    if ((event.memory_fault.fault_reason_mask & HSA_AMD_MEMORY_FAULT_SRAM_ECC) > 0)
    {
        err << "SRAM ECC failure;";
    }
    if ((event.memory_fault.fault_reason_mask & HSA_AMD_MEMORY_FAULT_HANG) > 0)
    {
        err << "GPU reset following unspecified hang;";
    }
    err << ")\n\n";
    AGENT_PRINT(err.str());
}
