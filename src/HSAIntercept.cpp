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

#include <unistd.h>
#include <fcntl.h>

// HSA headers
#include <hsa_api_trace.h>
#include <hsa_ven_amd_loader.h>

// Debug Agent Headers
#include "AgentLogging.h"
#include "AgentUtils.h"
#include "HSADebugAgent.h"
#include "HSADebugAgentGDBInterface.h"
#include "HSADebugInfo.h"
#include "HSAHandleMemoryFault.h"
#include "HSAHandleQueueError.h"
#include "HSAIntercept.h"

// The HSA Runtime's versions of HSA core API functions
CoreApiTable gs_OrigCoreApiTable;

// The HSA Runtime's versions of HSA ext API functions
AmdExtTable gs_OrigExtApiTable;

// The HSA Runtime's versions of HSA loader ext API functions
static hsa_ven_amd_loader_1_01_pfn_t gs_OrigLoaderExtTable;

// Update function table for intercepting
static void UpdateHSAFunctionTable(HsaApiTable* pTable);

// Intercept function hsa_queue_create
static hsa_status_t
HsaDebugAgentHsaQueueCreate(hsa_agent_t agent,
                               uint32_t size,
                               hsa_queue_type32_t type,
                               void (*callback)(hsa_status_t status,
                                                hsa_queue_t* source,
                                                void* data),
                               void* data,
                               uint32_t private_segment_size,
                               uint32_t group_segment_size,
                               hsa_queue_t** queue);

// Intercept function hsa_queue_destroy
static hsa_status_t
HsaDebugAgentHsaQueueDestroy(hsa_queue_t* queue);

// Intercept function hsa_executable_freeze
static hsa_status_t
HsaDebugAgentHsaExecutableFreeze(hsa_executable_t executable,
                                    const char *options);

// Intercept function hsa_executable_destroy
static hsa_status_t
HsaDebugAgentHsaExecutableDestroy(hsa_executable_t executable);

// Add executable info
static ExecutableInfo *
AddExecutableInfo(hsa_executable_t executable);

// Add loaded code object info
static hsa_status_t
AddExecCodeObjectInfo(hsa_executable_t executable, ExecutableInfo *pExecutable);

// Query loaded code object info for adding it in code object info link list
static hsa_status_t
AddCodeObjectInfoCallback(hsa_executable_t executable,
                          hsa_loaded_code_object_t loadedCodeObject,
                          void *data);

// Internal queue create callback
static void
HsaDebugAgentInternalQueueCreateCallback(const hsa_queue_t* queue,
                                         hsa_agent_t agent,
                                         void* data);

// This function will be extended with the kernel compilation interception too
DebugAgentStatus InitHsaCoreAgentIntercept(HsaApiTable* pTable)
{
    AGENT_LOG("InitHsaCoreAgentIntercept: Read HSA API Table");

    if (pTable == nullptr)
    {
        AGENT_ERROR("Interception: HSA Runtime provided a nullptr API Table");
        return DEBUG_AGENT_STATUS_FAILURE;
    }

    // This saves the original pointers
    memcpy(static_cast<void*>(&gs_OrigCoreApiTable),
           static_cast<const void*>(pTable->core_),
           sizeof(CoreApiTable));

    // This saves the original pointers
    memcpy(static_cast<void*>(&gs_OrigExtApiTable),
           static_cast<const void*>(pTable->amd_ext_),
           sizeof(AmdExtTable));

    hsa_status_t status = HSA_STATUS_SUCCESS;
    status = gs_OrigCoreApiTable.hsa_system_get_major_extension_table_fn(
            HSA_EXTENSION_AMD_LOADER,
            1,
            sizeof(hsa_ven_amd_loader_1_01_pfn_t),
            &gs_OrigLoaderExtTable);

    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot get loader extension function table");
        return DEBUG_AGENT_STATUS_FAILURE;
    }

    // Register internal queue create callbacks
    status = gs_OrigExtApiTable.hsa_amd_runtime_queue_create_register_fn(
            HsaDebugAgentInternalQueueCreateCallback, nullptr);
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot register internal queue create callback");
        return DEBUG_AGENT_STATUS_FAILURE;
    }

    // We override the table that we get from the runtime
    UpdateHSAFunctionTable(pTable);

    AGENT_LOG("InitHsaCoreAgentIntercept: Finished updating HSA API Table");
    return DEBUG_AGENT_STATUS_SUCCESS;
}

static void UpdateHSAFunctionTable(HsaApiTable* pTable)
{
    if (pTable == nullptr)
    {
        return;
    }

    AGENT_LOG("Interception: Replace functions with HSADebugAgent versions");

    pTable->core_->hsa_queue_create_fn = HsaDebugAgentHsaQueueCreate;
    pTable->core_->hsa_queue_destroy_fn = HsaDebugAgentHsaQueueDestroy;
    pTable->core_->hsa_executable_freeze_fn
            = HsaDebugAgentHsaExecutableFreeze;
    pTable->core_->hsa_executable_destroy_fn
            = HsaDebugAgentHsaExecutableDestroy;
}

static hsa_status_t
HsaDebugAgentHsaQueueCreate(
        hsa_agent_t agent,
        uint32_t size,
        hsa_queue_type32_t type,
        void (*callback)(hsa_status_t status, hsa_queue_t* source,
                         void* data),
        void* data,
        uint32_t private_segment_size,
        uint32_t group_segment_size,
        hsa_queue_t** queue)
{
    debugAgentAccessLock.lock();
    AGENT_LOG("Interception: hsa_queue_create");

    hsa_status_t status = HSA_STATUS_SUCCESS;

    uint32_t agentNode;
    status = gs_OrigCoreApiTable.hsa_agent_get_info_fn(agent, HSA_AGENT_INFO_NODE , &agentNode);
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Error when query agnet node: " << GetHsaStatusString(status));
        return status;
    }

    QueueInfo* pNewQueueInfo = new QueueInfo;
    pNewQueueInfo->queueStatus = HSA_STATUS_SUCCESS;
    pNewQueueInfo->callback = callback;
    pNewQueueInfo->data = data;
    pNewQueueInfo->pWaveList = nullptr;
    pNewQueueInfo->pPrev = nullptr;
    pNewQueueInfo->pNext = nullptr;

    status = gs_OrigCoreApiTable.hsa_queue_create_fn(agent,
                                                      size,
                                                      type,
                                                      HSADebugAgentHandleQueueError,
                                                      pNewQueueInfo,
                                                      private_segment_size,
                                                      group_segment_size,
                                                      queue);

    if (status != HSA_STATUS_SUCCESS || *queue == nullptr)
    {
        AGENT_ERROR("Interception: Cannot create a valid Queue, debugging will not work" <<
                    GetHsaStatusString(status));
        return status;
    }

    pNewQueueInfo->queue = (*queue);
    pNewQueueInfo->queueId = (**queue).id;

    DebugAgentStatus agentStatus = addQueueToList(agentNode, pNewQueueInfo);

    if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot add queue info to link list");
        return HSA_STATUS_ERROR;
    }

    AGENT_LOG("Interception: Exit hsa_queue_create");
    debugAgentAccessLock.unlock();

    return status;
}

static hsa_status_t
HsaDebugAgentHsaQueueDestroy(hsa_queue_t* queue)
{
    debugAgentAccessLock.lock();
    AGENT_LOG("Interception: hsa_queue_destroy");

    hsa_status_t status = HSA_STATUS_SUCCESS;

    RemoveQueueFromList(queue->id);

    status = gs_OrigCoreApiTable.hsa_queue_destroy_fn(queue);

    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Error when destory queue: " << GetHsaStatusString(status));
        return status;
    }

    AGENT_LOG("Interception: Exit hsa_queue_destroy");
    debugAgentAccessLock.unlock();

    return status;

}

static void
HsaDebugAgentInternalQueueCreateCallback(const hsa_queue_t* queue,
                                         hsa_agent_t agent,
                                         void* data)
{
    AGENT_LOG("Interception: internal queue create");

    hsa_status_t status = HSA_STATUS_SUCCESS;

    uint32_t agentNode;
    status = gs_OrigCoreApiTable.hsa_agent_get_info_fn(agent, HSA_AGENT_INFO_NODE , &agentNode);
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Error when query agnet node: " << GetHsaStatusString(status));
    }

    QueueInfo* pNewQueueInfo = new QueueInfo;
    pNewQueueInfo->queueStatus = HSA_STATUS_SUCCESS;
    pNewQueueInfo->callback = nullptr;
    pNewQueueInfo->data = nullptr;
    pNewQueueInfo->pWaveList = nullptr;
    pNewQueueInfo->pPrev = nullptr;
    pNewQueueInfo->pNext = nullptr;

    pNewQueueInfo->queue = (hsa_queue_t*)queue;
    pNewQueueInfo->queueId = queue->id;

    DebugAgentStatus agentStatus = addQueueToList(agentNode, pNewQueueInfo);
    if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot add queue info to link list");
    }

    AGENT_LOG("Interception: Exit internal queue create callback");
}

static hsa_status_t
HsaDebugAgentHsaExecutableFreeze(
        hsa_executable_t executable,
        const char *options)
{
    debugAgentAccessLock.lock();
    AGENT_LOG("Interception: hsa_executable_freeze");

    hsa_status_t status = HSA_STATUS_SUCCESS;
    status = gs_OrigCoreApiTable.hsa_executable_freeze_fn(
            executable,
            options);
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot freeze executable");
        return status;
    }

    // Create and add exec info to _r_rocm_debug_info
    ExecutableInfo *pExec;
    pExec = AddExecutableInfo(executable);
    if (pExec == nullptr)
    {
        AGENT_ERROR("Interception: Cannot add executable info");
        return HSA_STATUS_ERROR;
    }

    // Get loaded code object info of the freezed executable
    // and update _r_rocm_debug_info
    status = AddExecCodeObjectInfo(executable, pExec);
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot add code object info");
        return status;
    }

    // Update event info, nodeId will be updated when update code object info
    DebugAgentEventInfo *pEventInfo = _r_rocm_debug_info.pDebugAgentEvent;
    pEventInfo->eventType = DEBUG_AGENT_EVENT_EXECUTABLE_CREATE;
    pEventInfo->eventData.executableCreate.executableId = executable.handle;
    pEventInfo->eventData.executableCreate.executableHandle = (uint64_t)pExec;

    // Trigger GPU event breakpoint
    TriggerGPUEvent();

    AGENT_LOG("Interception: Exit hsa_executable_freeze");
    debugAgentAccessLock.unlock();

    return status;
}

static hsa_status_t
HsaDebugAgentHsaExecutableDestroy(
        hsa_executable_t executable)
{
    debugAgentAccessLock.unlock();
    AGENT_LOG("Interception: hsa_executable_destroy");

    hsa_status_t status = HSA_STATUS_SUCCESS;

    // Update event info
    DebugAgentEventInfo *pEventInfo = _r_rocm_debug_info.pDebugAgentEvent;
    pEventInfo->eventType = DEBUG_AGENT_EVENT_EXECUTABLE_DESTORY;
    pEventInfo->eventData.executableDestory.executableId = executable.handle;

    // Trigger GPU event breakpoint before remove it 
    TriggerGPUEvent();

    // Remove loaded code object info of the deleted executable
    // and update _r_rocm_debug_info
    DeleteExecutableFromList(executable.handle);

    status = gs_OrigCoreApiTable.hsa_executable_destroy_fn(
            executable);
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot destroy executable");
        return status;
    }

    AGENT_LOG("Interception: Exit hsa_executable_destroy");
    debugAgentAccessLock.unlock();

    return status;
}

static ExecutableInfo*
AddExecutableInfo(hsa_executable_t executable)
{
    AGENT_LOG("Interception: AddExecutableInfo");

    ExecutableInfo* pExec = new ExecutableInfo;
    pExec->executableId = executable.handle;
    pExec->nodeId = 0;    /* FIXME: executable can have code ojects of different agents, disabled for now */
    pExec->pCodeObjectList = nullptr;
    pExec->pPrev = nullptr;
    pExec->pNext = nullptr;

    DebugAgentStatus agentStatus = DEBUG_AGENT_STATUS_SUCCESS;

    agentStatus = AddExecutableToList(pExec);
    if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot add executable info to link list");
        delete pExec;
        return nullptr;
    }

    AGENT_LOG("Interception: Exit AddExecutableInfo");
    return pExec;
}

static hsa_status_t
AddExecCodeObjectInfo(hsa_executable_t executable, ExecutableInfo *pExec)
{
    hsa_status_t status = HSA_STATUS_SUCCESS;
    status = gs_OrigLoaderExtTable.
            hsa_ven_amd_loader_executable_iterate_loaded_code_objects(
                    executable,
                    AddCodeObjectInfoCallback,
                    (void *)pExec);
    return status;
}

static hsa_status_t
AddCodeObjectInfoCallback(
        hsa_executable_t executable,
        hsa_loaded_code_object_t loadedCodeObject,
        void *data)
{
    AGENT_LOG("Interception: AddCodeObjectInfoCallback");

    // TODO: add support to file based code object.
    hsa_status_t status = HSA_STATUS_SUCCESS;

    uint64_t elfBaseAddress;
    status = gs_OrigLoaderExtTable.hsa_ven_amd_loader_loaded_code_object_get_info(
            loadedCodeObject,
            HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_CODE_OBJECT_STORAGE_MEMORY_BASE,
            &elfBaseAddress);

    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Error when query HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_LOAD_BASE"
                << GetHsaStatusString(status));
        return status;
    }

    size_t elfSize;
    status = gs_OrigLoaderExtTable.hsa_ven_amd_loader_loaded_code_object_get_info(
            loadedCodeObject,
            HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_CODE_OBJECT_STORAGE_MEMORY_SIZE,
            &elfSize);

    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Error when query HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_LOAD_SIZE"
                << GetHsaStatusString(status));
        return status;
    }

    uint64_t loadedBaseAddress;
    status = gs_OrigLoaderExtTable.hsa_ven_amd_loader_loaded_code_object_get_info(
            loadedCodeObject,
            HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_LOAD_BASE,
            &loadedBaseAddress);

    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Error when query HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_CODE_OBJECT_STORAGE_MEMORY_BASE"
                << GetHsaStatusString(status));
        return status;
    }

    size_t loadedSize;
    status = gs_OrigLoaderExtTable.hsa_ven_amd_loader_loaded_code_object_get_info(
            loadedCodeObject,
            HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_LOAD_SIZE,
            &loadedSize);

    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Error when query HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_CODE_OBJECT_STORAGE_MEMORY_SIZE"
                << GetHsaStatusString(status));
        return status;
    }

    uint64_t addrDelta;
    status = gs_OrigLoaderExtTable.hsa_ven_amd_loader_loaded_code_object_get_info(
            loadedCodeObject,
            HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_LOAD_DELTA,
            &addrDelta);

    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Error when query HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_LOAD_DELTA"
                << GetHsaStatusString(status));
        return status;
    }

    // Query the agent of loaded code object and update node id of the executable, 
    // since the agent is undefined when it is created. Assuming all the code objects of 
    // an executable is for the same agent.
    hsa_agent_t loadedAgent;
    status = gs_OrigLoaderExtTable.hsa_ven_amd_loader_loaded_code_object_get_info(
            loadedCodeObject,
            HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_AGENT,
            &loadedAgent);

    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Error when query HSA_VEN_AMD_LOADER_LOADED_CODE_OBJECT_INFO_AGENT"
                << GetHsaStatusString(status));
        return status;
    }

    GPUAgentInfo *pAgent = GetAgentFromList(loadedAgent);
    ((ExecutableInfo *)data)->nodeId = pAgent->nodeId;

    CodeObjectInfo* pList = new CodeObjectInfo;
    pList->addrDelta = addrDelta;
    pList->addrLoaded = loadedBaseAddress;
    pList->sizeLoaded = loadedSize;
    pList->pPrev = nullptr;
    pList->pNext = nullptr;

    DebugAgentStatus agentStatus = DEBUG_AGENT_STATUS_SUCCESS;

    agentStatus = AddCodeObjectToList(pList, (ExecutableInfo *)data);
    if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot add code object info to link list");
        delete pList;
        return HSA_STATUS_ERROR;
    }

    agentStatus = SaveCodeObjectTempFile(elfBaseAddress, elfSize, pList);
    if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot save code object temp file");
        return HSA_STATUS_ERROR;
    }

    AGENT_LOG("Interception: Exit AddCodeObjectInfoCallback");

    return status;
}
