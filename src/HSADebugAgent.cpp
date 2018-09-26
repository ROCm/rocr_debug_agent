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

// HSA headers
#include <hsakmt.h>
#include <hsa_api_trace.h>

// Debug Agent Headers
#include "AgentLogging.h"
#include "AgentUtils.h"
#include "HSADebugAgentGDBInterface.h"
#include "HSADebugAgent.h"
#include "HSATrapHandler_s_gfx900.h"
#include "HSADebugInfo.h"
#include "HSAIntercept.h"
#include "HSAHandleLinuxSignals.h"

// Debug info tracked by debug agent, it is probed by ROCm-GDB
AmdGpuDebug _r_amd_gpu_debug;

// Temp direcoty path for code object files
char g_codeObjDir[92];

// whether delete tmp code object files
bool g_deleteTmpFile = true;

// Whether GDB is attached
bool g_gdbAttached = false;

// If debug agent is successfully loaded and initialized
bool g_debugAgentInitialSuccess = false;

// Debug trap signal used by trap handler
hsa_signal_t debugTrapSignal = {0};

// Debug trap handler code object reader
hsa_code_object_reader_t debugTrapHandlerCodeObjectReader = {0};

// Debug trap handler executable
hsa_executable_t debugTrapHandlerExecutable = {0};

// Debug trap handler buffer
DebugTrapBuff* pTrapHandlerBuffer = nullptr;

// Initial debug agent info link list
static DebugAgentStatus AgentInitDebugInfo();

// Get agent info in the callback when query agents
static hsa_status_t QueryAgentCallback(hsa_agent_t agent, void *pData);

// Get ISA info in the callback when query ISAs
static hsa_status_t QueryAgentISACallback(hsa_isa_t isa, void *pData);

// Check runtime version
static DebugAgentStatus AgentCheckVersion(uint64_t runtimeVersion,
                                          uint64_t failedToolCount,
                                          const char *const *pFailedToolNames);

// Check if ISA is supported by debug agent
static bool AgentIsSupportedISA(char *isaName);

// Clean _r_amd_gpu_debug
static void AgentCleanDebugInfo();

// Set debug trap handler through KFD
static DebugAgentStatus AgentSetDebugTrapHandler();

// Unset debug trap handler through KFD
static DebugAgentStatus AgentUnsetDebugTrapHandler();

// find debug trap handler of the agent
static void* FindDebugTrapHandler(char* pAgentName);

// find the kernarg segment when iterate regions
static hsa_status_t FindKernargSegment(hsa_region_t region, void *pData);

extern "C" bool OnLoad(void *pTable,
                       uint64_t runtimeVersion, uint64_t failedToolCount,
                       const char *const *pFailedToolNames)
{
    // TODO: may make this thread safe
    g_debugAgentInitialSuccess = false;
    DebugAgentStatus status = DEBUG_AGENT_STATUS_FAILURE;
    uint32_t tableVersionMajor =
            (reinterpret_cast<HsaApiTable *>(pTable))->version.major_id;
    uint32_t tableVersionMinor =
            (reinterpret_cast<HsaApiTable *>(pTable))->version.minor_id;

    status = AgentInitLogger();

    if (status != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Cannot initialize logging");
        return false;
    }

    AGENT_LOG("===== Load GDB Tools Agent=====");

    status = AgentCheckVersion(runtimeVersion, failedToolCount, pFailedToolNames);
    if (status != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Version mismatch");
        return false;
    }

    // Check function table version.
    if (tableVersionMajor < 1 || tableVersionMinor < 48)
    {
        AGENT_ERROR("Unsupported runtime version");
        return false;
    }

    status = AgentInitDebugInfo();
    if (status != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Cannot initialize debug info");
        return false;
    }

    status = AgentCreateTmpDir();
    if (status != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Cannot create code object directory");
        return false;
    }

    // Not available for ROCm1.9
    if (tableVersionMajor > 1)
    {
        status = AgentSetDebugTrapHandler();
        if (status != DEBUG_AGENT_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot set debug trap handler");
            return false;
        }
    }

    status = InitHsaCoreAgentIntercept(
            reinterpret_cast<HsaApiTable *>(pTable));

    if (status != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Cannot initialize dispatch tables");
        return false;
    }

    InitialLinuxSignalsHandler();

    AGENT_LOG("===== Finished Loading GDB Tools Agent=====");
    g_debugAgentInitialSuccess = true;
    return true;
}

extern "C" void OnUnload()
{
    AGENT_LOG("===== Unload GDB Tools Agent=====");

    DebugAgentStatus status = DEBUG_AGENT_STATUS_FAILURE;

    status = AgentCloseLogger();
    if (status != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("OnUnload: Cannot close Logging");
    }

    AgentCleanDebugInfo();

    status = AgentUnsetDebugTrapHandler();
    if (status != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("OnUnload: Cannot unset debug trap handler");
    }

}

// Check the version based on the provided by HSA runtime's OnLoad function.
// The logic is based on code in the HSA profiler (HSAPMCAgent.cpp).
// Return success if the versions match.
static DebugAgentStatus AgentCheckVersion(uint64_t runtimeVersion,
                                          uint64_t failedToolCount,
                                          const char *const *pFailedToolNames)
{
    DebugAgentStatus status = DEBUG_AGENT_STATUS_FAILURE;
    static const std::string ROCM_DEBUG_AGENT_LIB("libAMDHSADebugAgent-x64.so");

    if (failedToolCount > 0 && runtimeVersion > 0)
    {
        if (pFailedToolNames != nullptr)
        {
            for (uint64_t i = 0; i < failedToolCount; i++)
            {
                if (pFailedToolNames[i] != nullptr)
                {
                    std::string failedToolName = std::string(pFailedToolNames[i]);

                    if (std::string::npos != failedToolName.find_last_of(ROCM_DEBUG_AGENT_LIB))
                    {
                        AGENT_OP("rocm-gdb not enabled. Version mismatch between ROCm runtime and "
                                 << ROCM_DEBUG_AGENT_LIB);
                        AGENT_ERROR("Debug agent not enabled. Version mismatch between ROCm runtime and "
                                    << ROCM_DEBUG_AGENT_LIB);
                    }
                }
                else
                {
                    AGENT_ERROR("Debug agent not enabled," << ROCM_DEBUG_AGENT_LIB
                                                           << "version could not be verified");
                    AGENT_ERROR("AgentCheckVersion: pFailedToolNames[" << i << "] is nullptr");
                }
            }
            return status;
        }
        else
        {
            AGENT_ERROR("AgentCheckVersion: Cannot verify version successfully");
        }
    }
    else
    {
        status = DEBUG_AGENT_STATUS_SUCCESS;
    }

    return status;
}

static DebugAgentStatus AgentInitDebugInfo()
{
    AGENT_LOG("Initialize agent debug info")

    char *pGDBEnvVar;
    pGDBEnvVar = std::getenv("ROCM_ENABLE_GDB");
    if (pGDBEnvVar != NULL)
    {
        g_gdbAttached = true;
    }

    hsa_status_t status = HSA_STATUS_SUCCESS;

    _r_amd_gpu_debug = {HSA_DEBUG_AGENT_VERSION, nullptr, nullptr};

    GPUAgentInfo *pEndGPUAgentInfo = nullptr;
    status = hsa_iterate_agents(QueryAgentCallback, &(pEndGPUAgentInfo));
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Failed querying the device information.");
        return DEBUG_AGENT_STATUS_FAILURE;
    }

    AGENT_LOG("Finished initializing agent debug info")

    return DEBUG_AGENT_STATUS_SUCCESS;
}

static hsa_status_t QueryAgentCallback(hsa_agent_t agent, void *pData)
{
    // Add the GPU agent to the end of the agent list
    if (pData == nullptr)
    {
        AGENT_ERROR("QueryAgentCallback: Invalid argument pData");
        return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }

    int32_t status = HSA_STATUS_SUCCESS;
    // Find out the device type and skip it if it's a CPU.
    hsa_device_type_t deviceType;
    status = hsa_agent_get_info(
            agent, HSA_AGENT_INFO_DEVICE, &deviceType);
    if (status == HSA_STATUS_SUCCESS && deviceType == HSA_DEVICE_TYPE_CPU)
    {
        return HSA_STATUS_SUCCESS;
    }

    GPUAgentInfo *pGpuAgent = new GPUAgentInfo;
    memset(pGpuAgent, 0, sizeof(GPUAgentInfo));

    pGpuAgent->agent = agent;
    pGpuAgent->pQueueList = nullptr;
    pGpuAgent->agentStatus = AGENT_STATUS_UNSUPPORTED;

    char nameBuf[2 * AGENT_MAX_AGENT_NAME_LEN];
    memset(nameBuf, 0, 2 * AGENT_MAX_AGENT_NAME_LEN);
    status |= hsa_agent_get_info(
            agent, HSA_AGENT_INFO_VENDOR_NAME, nameBuf);
    // Insert a space between the vendor and product names.
    size_t vendorNameLen = strnlen(nameBuf, AGENT_MAX_AGENT_NAME_LEN);
    strncpy(nameBuf + vendorNameLen, " ", 1);

    status |= hsa_agent_get_info(
            agent, HSA_AGENT_INFO_NAME, nameBuf + vendorNameLen + 1);
    memcpy(pGpuAgent->agentName, nameBuf, AGENT_MAX_AGENT_NAME_LEN);

    // TODO: HSA_AGENT_INFO_NODE is deprecated.
    // Check if HSA_AMD_AGENT_INFO_DRIVER_NODE_ID can be used instead.
    status |= hsa_agent_get_info(
            agent,
            static_cast<hsa_agent_info_t>(HSA_AGENT_INFO_NODE),
            &(pGpuAgent->nodeId));
    status |= hsa_agent_get_info(
            agent,
            static_cast<hsa_agent_info_t>(HSA_AMD_AGENT_INFO_CHIP_ID),
            &(pGpuAgent->chipID));
    status |= hsa_agent_get_info(
            agent,
            static_cast<hsa_agent_info_t>(HSA_AMD_AGENT_INFO_COMPUTE_UNIT_COUNT),
            &(pGpuAgent->numCUs));
    status |= hsa_agent_get_info(
            agent,
            static_cast<hsa_agent_info_t>(HSA_AMD_AGENT_INFO_NUM_SHADER_ENGINES),
            &(pGpuAgent->numSEs));
    status |= hsa_agent_get_info(
            agent,
            static_cast<hsa_agent_info_t>(HSA_AMD_AGENT_INFO_NUM_SIMDS_PER_CU),
            &(pGpuAgent->numSIMDsPerCU));
    status |= hsa_agent_get_info(
            agent,
            static_cast<hsa_agent_info_t>(HSA_AMD_AGENT_INFO_MAX_WAVES_PER_CU),
            &(pGpuAgent->wavesPerCU));
    status |= hsa_agent_get_info(
            agent,
            static_cast<hsa_agent_info_t>(HSA_AMD_AGENT_INFO_MAX_CLOCK_FREQUENCY),
            &(pGpuAgent->maxEngineFreq));
    status |= hsa_agent_get_info(
            agent,
            static_cast<hsa_agent_info_t>(HSA_AMD_AGENT_INFO_MEMORY_MAX_FREQUENCY),
            &(pGpuAgent->maxMemoryFreq));
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_WARNING("Failed to get some of the device info");
    }

    status = hsa_agent_iterate_isas(
            agent, QueryAgentISACallback, pGpuAgent);
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Cannot get supported ISA(s) for agent "
                    << pGpuAgent->agentName);
        return HSA_STATUS_ERROR;
    }

    if (pGpuAgent->agentStatus != AGENT_STATUS_ACTIVE)
    {
        AGENT_WARNING("Do not support agent " << pGpuAgent->agentName);
    }

    GPUAgentInfo *pPrevGPUAgent = *(GPUAgentInfo **)pData;
    pGpuAgent->pPrev = pPrevGPUAgent;
    if (pPrevGPUAgent == nullptr)
    {
        _r_amd_gpu_debug.pAgentList = pGpuAgent;
    }
    else
    {
        pPrevGPUAgent->pNext = pGpuAgent;
    }
    // Update pData to point to the end of the link list,
    // and pass through agent iteration.
    *(GPUAgentInfo **)pData = pGpuAgent;

    return HSA_STATUS_SUCCESS;
}

static hsa_status_t QueryAgentISACallback(hsa_isa_t isa, void *pData)
{
    if (pData == nullptr)
    {
        return HSA_STATUS_ERROR;
    }

    char isaName[AGENT_MAX_AGENT_NAME_LEN];
    //TODO: check isa name length
    hsa_status_t status = hsa_isa_get_info_alt(
            isa, HSA_ISA_INFO_NAME, isaName);
    if (AgentIsSupportedISA(isaName))
    {
        ((GPUAgentInfo *)pData)->agentStatus = AGENT_STATUS_ACTIVE;
    }
    return status;
}

// TODO: should be a better way to check isa version,
// as the naming could change.
static bool AgentIsSupportedISA(char *isaName)
{
    if (strcmp(isaName, gfx900) == 0)
    {
        return true;
    }
    return false;
}

static void AgentCleanDebugInfo()
{
    GPUAgentInfo *pAgent = _r_amd_gpu_debug.pAgentList;
    GPUAgentInfo *pAgentNext = nullptr;

    debugInfoLock.lock();
    while (pAgent != nullptr)
    {
        pAgentNext = pAgent->pNext;
        QueueInfo *pQueue = pAgent->pQueueList;
        QueueInfo *pQueueListNext = nullptr;
        while (pQueue != nullptr)
        {
            pQueueListNext = pQueue->pNext;
            CleanUpQueueWaveState(pAgent->nodeId, pQueue->queueId);
            RemoveQueueFromList(pQueue->queueId);
            pQueue = pQueueListNext;
        }
        pAgent = pAgentNext;
    }
    debugInfoLock.unlock();

    codeObjectInfoLock.lock();
    CodeObjectInfo *pCodeObj = _r_amd_gpu_debug.pCodeObjectList;
    CodeObjectInfo *pCodeObjNext = nullptr;
    while (pCodeObj != nullptr)
    {
        pCodeObjNext = pCodeObj->pNext;
        RemoveCodeObjectFromList(pCodeObj->addrLoaded);
        pCodeObj = pCodeObjNext;
    }
    codeObjectInfoLock.unlock();

    if (g_deleteTmpFile)
    {
        AgentDeleteFile(g_codeObjDir);
    }
}

static void* FindDebugTrapHandler(char* pAgentName)
{
    if (pAgentName == nullptr)
    {
        return nullptr;
    }

    // The name check is based on the format of "AMD gfx900"
    if (strncmp(&(pAgentName[4]), "gfx", 3) != 0)
    {
        return nullptr;
    }

    uint64_t targetName = atoi(&(pAgentName[7]));
    uint8_t gfxMajor = targetName / 100;

    switch (gfxMajor) {
        case 9:
            return HSATrapHandler_s_gfx900_co;
        default:
            return nullptr;
    }
}

static DebugAgentStatus AgentSetDebugTrapHandler()
{
    GPUAgentInfo* pAgent = _r_amd_gpu_debug.pAgentList;
    GPUAgentInfo* pAgentNext = nullptr;

    while (pAgent != nullptr)
    {
        void* HSATrapHandler = nullptr;
        void* pTrapHandlerEntry = nullptr;
        DebugTrapBuff* pTrapHandlerBuffer = nullptr;
        uint64_t trapHandlerSize = 0;
        uint64_t trapHandlerBufferSize = 0;
        const char* pEntryPointName = "debug_trap_handler";
        uint64_t kernelCodeAddress = 0;
        hsa_region_t kernargSegment = {0};
        hsa_executable_symbol_t symbol = {0};
        hsa_status_t status = HSA_STATUS_SUCCESS;

        // Find target trap handler
        HSATrapHandler = FindDebugTrapHandler(pAgent->agentName);
        if (HSATrapHandler == nullptr)
        {
            AGENT_ERROR("Cannot find debug trap handler for agent "
                        << pAgent->agentName);
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        status = hsa_code_object_reader_create_from_memory(
                HSATrapHandler, sizeof(HSATrapHandler), &debugTrapHandlerCodeObjectReader);
        if (status != HSA_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot create debug trap handler code object.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        // Create executable.
        status = hsa_executable_create_alt(
            HSA_PROFILE_FULL, HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT, NULL, &debugTrapHandlerExecutable);
        if (status != HSA_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot create debug trap handler executable.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        // Load debug trap handler code object
        status = hsa_executable_load_agent_code_object(
                debugTrapHandlerExecutable, pAgent->agent, debugTrapHandlerCodeObjectReader, NULL, NULL);
        if (status != HSA_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot load debug trap handler code object.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        // freeze executable
        status = hsa_executable_freeze(debugTrapHandlerExecutable, NULL);
        if (status != HSA_STATUS_SUCCESS)
        {
            AGENT_ERROR("Can freeze debug trap handler executable.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        // find the kernel symbol.
        status = hsa_executable_get_symbol_by_name(
                debugTrapHandlerExecutable, pEntryPointName, &(pAgent->agent), &symbol);
        if (status != HSA_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot find debug trap handler symbol.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        status = hsa_executable_symbol_get_info(
                symbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &kernelCodeAddress);
        if (status != HSA_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot get debug trap handler entry point address.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        // hsa_executable_get_symbol_by_name with kernel name
        // get the address of amd_kernel_code_t in ROCm 1.9
        // TODO: access the code enty offset in kernerl discriptor
        pTrapHandlerEntry = (void*)(kernelCodeAddress + 256);

        // Check pTrapHandlerEntry is multiple of 256
        if(!IsMultipleOf(pTrapHandlerEntry, 0x100))
        {
            AGENT_ERROR("pTrapHandlerEntry is not 256B aligned.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        status = hsa_signal_create(
                0, 0, &(pAgent->agent), &debugTrapSignal);
        if (status != HSA_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot create debug event signal.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        // find kernarg segment
        // TODO: put the trap buffer in device memory for efficiency
        //       use the copy APIs to update the value
        status = hsa_agent_iterate_regions(
                pAgent->agent, FindKernargSegment, &kernargSegment);
        if (!kernargSegment.handle | (status != HSA_STATUS_SUCCESS)) {
            AGENT_ERROR("Cannot find kernarg segment for trap buffer.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        // allocate memory in kernarg segment for trap buffer
        status = hsa_memory_allocate(
                kernargSegment, sizeof(DebugTrapBuff), (void**)&pTrapHandlerBuffer);
        if (!kernargSegment.handle | (status != HSA_STATUS_SUCCESS)) {
            AGENT_ERROR("Cannot allocate memory for trap buffer.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }
        memset(pTrapHandlerBuffer, 0, sizeof(DebugTrapBuff));

        pTrapHandlerBuffer->debugEventSignalHandle = debugTrapSignal.handle;
        if(!IsMultipleOf(pTrapHandlerBuffer, 0x100))
        {
            AGENT_ERROR("Trap Handler Buffer address is not 256B aligned.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        trapHandlerBufferSize = sizeof(DebugTrapBuff);

        // Register trap handler in KFD
        HSAKMT_STATUS kmtStatus = hsaKmtSetTrapHandler(
                pAgent->nodeId, pTrapHandlerEntry,
                trapHandlerSize, pTrapHandlerBuffer, trapHandlerBufferSize);
        if (kmtStatus != HSAKMT_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot register debug trap handler.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        // Bind trap handler event signal in runtime
        status = hsa_amd_signal_async_handler(
                debugTrapSignal, HSA_SIGNAL_CONDITION_NE, 0,
                NULL, NULL);

        pAgentNext = pAgent->pNext;
        pAgent = pAgentNext;
    }

    return DEBUG_AGENT_STATUS_SUCCESS;
}

static DebugAgentStatus AgentUnsetDebugTrapHandler()
{
    GPUAgentInfo* pAgent = _r_amd_gpu_debug.pAgentList;
    GPUAgentInfo* pAgentNext = nullptr;

    while (pAgent != nullptr)
    {
        hsa_status_t status = HSA_STATUS_SUCCESS;

        status = hsa_executable_destroy(debugTrapHandlerExecutable);
        if (status != HSA_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot destroy debug trap handler executable.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        status = hsa_code_object_reader_destroy(debugTrapHandlerCodeObjectReader);
        if (status != HSA_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot destroy debug trap handler code object reader.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        status = hsa_signal_destroy(debugTrapSignal);
        if (status != HSA_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot destroy debug event signal.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        status = hsa_memory_free((void*)pTrapHandlerBuffer);
        if (status != HSA_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot destroy debug event signal.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        // Reset debug trap handler by regsiter nullptr
        HSAKMT_STATUS kmtStatus = hsaKmtSetTrapHandler(
                pAgent->nodeId, nullptr,
                0, nullptr, 0);
        if (kmtStatus != HSAKMT_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot reset debug trap handler.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }

        pAgentNext = pAgent->pNext;
        pAgent = pAgentNext;
    }

    return DEBUG_AGENT_STATUS_SUCCESS;
}

hsa_status_t FindKernargSegment(hsa_region_t region, void *pData)
{
    if (!pData)
    {
        return HSA_STATUS_ERROR_INVALID_ARGUMENT;
    }

    hsa_region_segment_t seg;
    hsa_status_t status  = hsa_region_get_info(region, HSA_REGION_INFO_SEGMENT, &seg);
    if (status != HSA_STATUS_SUCCESS)
    {
        return status;
    }
    if (seg != HSA_REGION_SEGMENT_GLOBAL)
    {
        return HSA_STATUS_SUCCESS;
    }

    uint32_t flags;
    status = hsa_region_get_info(region, HSA_REGION_INFO_GLOBAL_FLAGS, &flags);
    if (status != HSA_STATUS_SUCCESS)
    {
        return status;
    }

    if (flags & HSA_REGION_GLOBAL_FLAG_KERNARG)
    {
        *((hsa_region_t*)pData) = region;
    }

    return HSA_STATUS_SUCCESS;
}
