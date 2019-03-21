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

#ifndef HSA_DEBUG_INFO_H_
#define HSA_DEBUG_INFO_H_

#include <map>
#include <vector>

// Debug Agent Headers
#include "HSADebugAgent.h"

// forward declaration.
typedef struct _GPUAgentInfo GPUAgentInfo;
typedef struct _QueueInfo QueueInfo;
typedef struct _WaveStateInfo WaveStateInfo;
typedef struct _CodeObjectInfo CodeObjectInfo;
typedef struct _ExecutableInfo ExecutableInfo;
typedef struct _DebugAgentQueueInfo DebugAgentQueueInfo;

// Waves of a queue.
typedef std::vector<WaveStateInfo> WaveStates;

// Map of all queues with their waves <queueId, waves>.
typedef std::map<uint64_t, WaveStates> DebugAgentQueueWaveMap;

// Map of all queues with additional info needed in debug agent
// <queueId, DebugAgentQueueInfo>.
typedef std::map<uint64_t, DebugAgentQueueInfo> DebugAgentQueueInfoMap;

#define COMPUTE_RELAUNCH_PAYLOAD_VGPRS(x) (((x) >> 0x0) & 0x3F)
#define COMPUTE_RELAUNCH_PAYLOAD_SGPRS(x) (((x) >> 0x6) & 0x7)
#define COMPUTE_RELAUNCH_PAYLOAD_LDS_SIZE(x) (((x) >> 0x9) & 0x1FF)
#define COMPUTE_RELAUNCH_PAYLOAD_FIRST_WAVE(x) (((x) >> 0x11) & 0x1)
#define COMPUTE_RELAUNCH_IS_EVENT(x) (((x) >> 0x1E) & 0x1)
#define COMPUTE_RELAUNCH_IS_STATE(x) (((x) >> 0x1F) & 0x1)
#define SQ_WAVE_TRAPSTS_XNACK_ERROR(x) (((x) >> 0x1C) & 0x1)

// Wave states of a queue for ROCmGDB to probe.
typedef struct _WaveStateInfo
{
    // Number of SGPRs allocated per wavefront.
    uint32_t numSgprs;
    // Array of packed SGPR data.
    uint32_t* sgprs;
    // Number of VGPRs allocated per wavefront.
    uint32_t numVgprs;
    // Number of lanes in each VGPR.
    uint32_t numVgprLanes;
    // Array of packed VGPR data.
    // VGPR value = vgprs[(vgprIdx * numVgprLanes) + laneIdx]
    uint32_t* vgprs;
    // LDS allocation size for the work group, in 32bit words.
    uint32_t ldsSizeDw;
    // Packed LDS data for the work group.
    uint32_t* lds;
    // Data for miscellaneous registers.
    struct
    {
        uint64_t pc;
        uint64_t exec;
        uint32_t status;
        uint32_t trapsts;
        uint32_t m0;
    } regs;
} WaveStateInfo;

// Queue info only used by debug agent
typedef struct _DebugAgentQueueInfo
{
    // Orignal callback registered by the HSA runtime for asynchronous event.
    void* callback;
    // Orignal application data that is passed to the callback.
    void* data;
} DebugAgentQueueInfo;

extern DebugAgentQueueWaveMap allQueueWaves;
extern DebugAgentQueueInfoMap allDebugAgentQueueInfo;

// Get the wave states of a queue from context save area,
// decode and update wave info in the queue.
DebugAgentStatus ProcessQueueWaveStates(uint32_t nodeId, uint64_t queueId);

// Preempt queues of all agents and process the waves.
DebugAgentStatus PreemptAllQueues();

// Preempt queues of an agent.
DebugAgentStatus PreemptAgentQueues(GPUAgentInfo* pAgent);

// Preempt queues of all agent that are not in error state.
DebugAgentStatus ResumeAllQueues();

// Resume queues of an agent.
DebugAgentStatus ResumeAgentQueues(GPUAgentInfo* pAgent);

// Print waves based on its PC.
void PrintWaves(GPUAgentInfo* pAgent, std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> waves);

// Get the pointer of specified agent info by node from the agent link list.
GPUAgentInfo* GetAgentFromList(uint32_t node);

// Get the pointer of specified agent info by agent handle from the agent link list.
GPUAgentInfo* GetAgentFromList(void* agentHandle);

// Get the pointer of specified agent info by a queue id of it.
GPUAgentInfo* GetAgentByQueueID(uint64_t queueId);

// Add queue info to the link list.
DebugAgentStatus addQueueToList(uint32_t nodeId, QueueInfo* pQueue);

// Get the pointer of specified queue info by node and queue id from the link list.
QueueInfo* GetQueueFromList(uint32_t node, uint64_t queueId);
QueueInfo* GetQueueFromList(uint64_t queueId);

// Add executable info to the link list.
DebugAgentStatus AddExecutableToList(ExecutableInfo* pExec);

// Get the pointer of executable info by its id.
ExecutableInfo *GetExecutableFromList(uint64_t execId);

// Delete executable info from link list.
void DeleteExecutableFromList(uint64_t execId);

// Add code object info to the link list.
DebugAgentStatus AddCodeObjectToList(CodeObjectInfo* pCodeObject, ExecutableInfo * pExecutable);

// Delete the code object info by its loaded address from link list.
void DeleteCodeObjectFromList(uint64_t addrLoaded, ExecutableInfo * pExecutable);

// Remove the queue info by its queue id from link list.
void RemoveQueueFromList(uint64_t queueId);

// Return pointer to the end element of a link list
template <class listType>
listType* GetLinkListEnd(listType* pList)
{
    if (pList == nullptr)
    {
      return nullptr;
    }
    while(pList->pNext != nullptr)
    {
      pList = pList->pNext;
    }
    return pList;
};

// Add element to the end of a link list
template <class listType>
DebugAgentStatus AddToLinkListEnd(listType* pAddItem, listType** ppList)
{
    if (ppList == nullptr)
    {
       return DEBUG_AGENT_STATUS_FAILURE;
    }

    if (*ppList == nullptr)
    {
        *ppList = pAddItem;
    }
    else
    {
        listType* pList = *ppList;
        while(pList->pNext != nullptr)
        {
            pList = pList->pNext;
        }
        pList->pNext = pAddItem;
        pAddItem->pPrev = pList;
    }
    return DEBUG_AGENT_STATUS_SUCCESS;
};

#endif // HSA_DEBUG_INFO_H_
