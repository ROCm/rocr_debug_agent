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

#ifndef DEBUG_AGENT_GDB_INTERFACE_H_
#define DEBUG_AGENT_GDB_INTERFACE_H_

#define HSA_DEBUG_AGENT_VERSION 1
#define AGENT_MAX_AGENT_NAME_LEN  64
#define AGENT_MAX_FILE_PATH_LEN 128
// Max breakpoint number based on the trap buffer size (0x1000)
#define AGENT_MAX_BREAKPOINT 510

extern "C" {

// HSA agent status
// TODO: include the agent error state if available.
typedef enum
{
    // Normal agent status
    AGENT_STATUS_ACTIVE = 0,
    // Unsupported agent
    AGENT_STATUS_UNSUPPORTED,
    // Memory fault in supported agent
    AGENT_STATUS_MEMORY_FAULT,
    // Memory fault in unsupported agent
    AGENT_STATUS_UNSUPPORTED_MEMORY_FAULT,
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

// Link list that maintains loaded code objects for ROCm-GDB to probe.
typedef struct _CodeObjectInfo
{
    // Difference between the address in the ELF file and the addresses in memory.
    uint64_t addrDelta;
    // Absolute temp code object file path.
    char path[AGENT_MAX_FILE_PATH_LEN];
    // Loaded address of the code object.
    size_t addrLoaded;
    // Loaded size of the code object.
    size_t sizeLoaded;
    // Chain of loaded objects.
    struct _CodeObjectInfo* pNext;
    struct _CodeObjectInfo* pPrev;
} CodeObjectInfo;

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

// Link list that maintains wave states of a queue for ROCm-GDB to probe.
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
    // LDS allocation size for the work group, in 32-bit words.
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
    // Next element of the wave state link list.
    struct _WaveStateInfo* pNext;
    // Previous element of the wave state link list.
    struct _WaveStateInfo* pPrev;

} WaveStateInfo;

// List list that maintains queues of an agent for ROCm-GDB to probe.
typedef struct _QueueInfo
{
    // Queue status. Value is QueueStatus. TODO: hold runtime queue error state
    uint64_t queueStatus;
    // Pointer to the queue.
    hsa_queue_t* queue;
    // Runtime queue id.
    uint64_t queueId;
    // Orignal callback registered by the HSA runtime for asynchronous event.
    void (*callback)(hsa_status_t status, hsa_queue_t* source, void* data);
    // Orignal application data that is passed to the callback.
    void* data;
    // Link list of wave states of the queue
    WaveStateInfo* pWaveList;
    // Next element of the queue link list.
    struct _QueueInfo* pNext;
    // Previous element of the queue link list.
    struct _QueueInfo* pPrev;
} QueueInfo;

// List list that maintains agents for ROCm-GDB to probe.
typedef struct _GPUAgentInfo
{
    // Agent status. Vaule is AgentStatus.
    uint64_t agentStatus;
    // GPU Agent handle.
    hsa_agent_t agent;
    // Agent node id.
    uint64_t nodeId;
    // Agent name.
    char     agentName[AGENT_MAX_AGENT_NAME_LEN];
    // Chip identifier.
    uint64_t chipID;
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
    // The GPU memory fault info. Only valid when agentStatus is AGENT_STATUS_MEMORY_FAULT.
    MemoryFaultInfo memoryFaultInfo;
    // Link list of queues of the agent.
    QueueInfo* pQueueList;
    // Next element of the agent link list.
    struct _GPUAgentInfo* pNext;
    // Previous element of the agent link list.
    struct _GPUAgentInfo* pPrev;
} GPUAgentInfo;

// Struct that maintains all debug info for ROCm-GDB to probe.
typedef struct _AmdGpuDebug
{
    // Version number for the debug agent.
    uint64_t version;
    // Head of the chain of AMD GPU agents.
    GPUAgentInfo* pAgentList;
    // Head of the chain of loaded objects.
    CodeObjectInfo* pCodeObjectList;
} AmdGpuDebug;

// Debug trap handler buffer struct
typedef struct _DebugTrapBuff
{
    uint64_t debugEventSignalHandle;
    uint32_t numMaxBreakPoint = AGENT_MAX_BREAKPOINT;
    uint32_t numCurrectBreakPoint;
    uint64_t breakPointPC[AGENT_MAX_BREAKPOINT];
} DebugTrapBuff;

/// GDB will install a breakpoint on this function that will be used when
/// a GPU kernel breakpoint is hit.
/// It is defined as extern C to facilitate the name lookup by GDB. This
/// could be changed to use exported symbol referring to locations
/// as it is done in the in-process agent library.

// Regular GPU breakpoint
void __attribute__((optimize("O0"))) TriggerGPUUserBreakpoint(void);

// Breakpoint for code object update
void __attribute__((optimize("O0"))) TriggerGPUCodeObjectUpdate(void);

// Breakpoint for queue update
void __attribute__((optimize("O0"))) TriggerGPUQueueUpdate(void);

// Breakpoint for GPU fault
void __attribute__((optimize("O0"))) TriggerGPUEventFault(void);

} // extern "C"
#endif // DEBUG_AGENT_GDB_INTERFACE_H_
