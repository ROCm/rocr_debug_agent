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
#define SQ_WAVE_TRAPSTS_MEM_VIOL(x) (((x) >> 0x8) & 0x1)

#define HSA_DEBUG_AGENT_VERSION 1
#define AGENT_MAX_AGENT_NAME_LEN  64
#define AGENT_MAX_FILE_PATH_LEN 128

// HSA agent status
// TODO: include the agent error state if available.
typedef enum
{
    // Normal agent status
    AGENT_STATUS_ACTIVE = 0,
    // Unsupported agent
    AGENT_STATUS_UNSUPPORTED,
} AgentStatus;

// HSA queue status
// TODO: include the runtime queue error state.
typedef enum
{
    // Normal queue status
    QUEUE_STATUS_ACTIVE = 0,
    // Failure in the queue
    QUEUE_STATUS_FAILURE,
} QueueStatus;

// Loaded code objects for ROCm-GDB to probe.
typedef struct _CodeObjectInfo
{
    // Difference between the address in the ELF file and the addresses in memory.
    uint64_t addrDelta;
    // Absolute temp code object file path.
    char path[AGENT_MAX_FILE_PATH_LEN];
    // Address of the ELF file in host memory.
    uint64_t addrMemory;
    // Size of the ELF file in host memory.
    size_t sizeMemory;
    // Loaded address of the code object.
    uint64_t addrLoaded;
    // Loaded size of the code object.
    size_t sizeLoaded;
    // Chain of loaded objects.
    struct _CodeObjectInfo* pNext;
    struct _CodeObjectInfo* pPrev;
} CodeObjectInfo;

// Executables for ROCm-GDB to probe.
typedef struct _ExecutableInfo
{
    uint64_t executableId;
    uint32_t nodeId;
    CodeObjectInfo* pCodeObjectList;
    struct _ExecutableInfo* pNext;
    struct _ExecutableInfo* pPrev;
} ExecutableInfo;

// TODO: define fault reason mask type
// Memory fault info for ROCm-GDB to probe
typedef struct _MemoryFaultInfo
{
    // Virtual address accessed.
    uint64_t virtualAddress;
    // Bit field encoding the memory access failure reasons.
    // There could be multiple bits set for one fault.
    // 0x00000001 Page not present or supervisor privilege.
    // 0x00000010 Write access to a read-only page.
    // 0x00000100 Execute access to a page marked NX.
    // 0x00001000 Host access only.
    // 0x00010000 ECC failure (if supported by HW).
    // 0x00100000 Can't determine the exact fault address.
    uint32_t faultReasonMask;
} MemoryFaultInfo;

// Queues of an agent for ROCm-GDB to probe.
typedef struct _QueueInfo
{
    // Queue status. Value is QueueStatus. TODO: hold runtime queue error state
    uint64_t queueStatus;
    // Pointer to the queue.
    void*    queue;
    // Runtime queue id.
    uint64_t queueId;
    // Agent node id the queue belongs to.
    uint32_t nodeId;
    // Agent gpu_id the queue belongs to.
    uint32_t gpuId;
    // Context save area
    void* pContextSaveArea;
    // Context save area size
    uint32_t contextSaveAreaSize;
    // Control stack
    void* pControlStack;
    // Control stack size
    uint32_t controlStackSize;
    // HsaUserContextSaveAreaHeader header
    void* pSaveAreaHeader;
    // Next element of the queue link list.
    struct _QueueInfo* pNext;
    // Previous element of the queue link list.
    struct _QueueInfo* pPrev;
} QueueInfo;

// Agents for ROCm-GDB to probe.
typedef struct _GPUAgentInfo
{
    // Agent status. Vaule is AgentStatus.
    uint64_t agentStatus;
    // GPU Agent handle.
    void*    agent;
    // Agent node id.
    uint32_t nodeId;
    // Agent gpu_id.
    uint32_t gpuId;
    // Agent name.
    char     agentName[AGENT_MAX_AGENT_NAME_LEN];
    // Chip identifier.
    uint32_t chipID;
    // Num of compute units.
    uint32_t numCUs;
    // The maximum clock frequency of the agent in MHz.
    uint32_t maxEngineFreq;
    // Max Memory Clock.
    uint32_t maxMemoryFreq;
    // Maximum number of waves possible. in a Compute Unit.
    uint32_t wavesPerCU;
    // Number of SIMD's per compute unit.
    uint32_t numSIMDsPerCU;
    // num of shader engines.
    uint32_t numSEs;
    // has acc vgprs.
    bool hasAccVgprs;
    // Link list of queues of the agent.
    QueueInfo* pQueueList;
    // Next element of the agent link list.
    struct _GPUAgentInfo* pNext;
    // Previous element of the agent link list.
    struct _GPUAgentInfo* pPrev;
} GPUAgentInfo;

// Struct that maintains all debug info for ROCm-GDB to probe.
typedef struct _RocmGpuDebug
{
    // Version number for the debug agent.
    uint64_t version;
    // Head of the chain of AMD GPU agents.
    GPUAgentInfo* pAgentList;
    // Head of the chain of loaded objects.
    ExecutableInfo* pExecutableList;
} RocmGpuDebug;

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
    // Number of VGPRs allocated per wavefront.
    uint32_t numAccVgprs;
    // Number of lanes in each VGPR.
    uint32_t numAccVgprLanes;
    // Array of packed VGPR data.
    // VGPR value = vgprs[(vgprIdx * numVgprLanes) + laneIdx]
    uint32_t* vgprs;
    uint32_t* accvgprs;
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
DebugAgentStatus ProcessQueueWaveStates(GPUAgentInfo* pAgent, QueueInfo *pQueue);

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
