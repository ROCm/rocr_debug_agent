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
#include <hsa.h>
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
HsaDebugAgent_hsa_queue_create(hsa_agent_t agent,
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
HsaDebugAgent_hsa_queue_destroy(hsa_queue_t* queue);

// Intercept function hsa_executable_freeze
static hsa_status_t
HsaDebugAgent_hsa_executable_freeze(hsa_executable_t executable,
                                    const char *options);

// Intercept function hsa_executable_destroy
static hsa_status_t
HsaDebugAgent_hsa_executable_destroy(hsa_executable_t executable);

// Add loaded code object info when loading
static hsa_status_t
AddExecCodeObjectInfo(hsa_executable_t executable);

// Query loaded code object info for adding it in code object info link list
static hsa_status_t
AddCodeObjectInfoCallback(hsa_executable_t executable,
                          hsa_loaded_code_object_t loadedCodeObject,
                          void *data);

// Delete code object info when unloading
static hsa_status_t
DeleteExecCodeObjectInfo(hsa_executable_t executable);

// Query loaded code object info for deleting it from code object info link list
static hsa_status_t
DeleteCodeObjectInfoCallback(hsa_executable_t executable,
                             hsa_loaded_code_object_t loadedCodeObject,
                             void *data);

// Register custom event handler in runtime.
static hsa_status_t InitEventHandler();

// Handle runtime event based on event type.
static hsa_status_t
HSADebugAgentHandleRuntimeEvent(const hsa_amd_event_t* event, void* pData);

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

    // We override the table that we get from the runtime
    UpdateHSAFunctionTable(pTable);

    // Set the custom runtime event handler
    status = InitEventHandler();
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot register GPU event handler");
        return DEBUG_AGENT_STATUS_FAILURE;
    }

    AGENT_LOG("InitHsaCoreAgentIntercept: Finished updating HSA API Table");
    return DEBUG_AGENT_STATUS_SUCCESS;
}

hsa_status_t InitEventHandler()
{
    hsa_status_t status = HSA_STATUS_SUCCESS;
    status = gs_OrigExtApiTable.hsa_amd_register_system_event_handler_fn(HSADebugAgentHandleRuntimeEvent, NULL);
    return status;
}

static void UpdateHSAFunctionTable(HsaApiTable* pTable)
{
    if (pTable == nullptr)
    {
        return;
    }

    AGENT_LOG("Interception: Replace functions with HSADebugAgent versions");

    pTable->core_->hsa_queue_create_fn = HsaDebugAgent_hsa_queue_create;
    pTable->core_->hsa_queue_destroy_fn = HsaDebugAgent_hsa_queue_destroy;
    pTable->core_->hsa_executable_freeze_fn
            = HsaDebugAgent_hsa_executable_freeze;
    pTable->core_->hsa_executable_destroy_fn
            = HsaDebugAgent_hsa_executable_destroy;
}

static hsa_status_t
HsaDebugAgent_hsa_queue_create(
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

    debugInfoLock.lock();
    DebugAgentStatus agentStatus = addQueueToList(agentNode, pNewQueueInfo);
    debugInfoLock.unlock();

    if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot add queue info to link list");
        return HSA_STATUS_ERROR;
    }

    // Trigger GPU event breakpoint
    TriggerGPUQueueUpdate();

    AGENT_LOG("Interception: Exit hsa_queue_create");

    return status;
}

static hsa_status_t
HsaDebugAgent_hsa_queue_destroy(hsa_queue_t* queue)
{

    AGENT_LOG("Interception: hsa_queue_destroy");

    hsa_status_t status = HSA_STATUS_SUCCESS;

    debugInfoLock.lock();
    RemoveQueueFromList(queue->id);
    debugInfoLock.unlock();

    status = gs_OrigCoreApiTable.hsa_queue_destroy_fn(queue);

    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Error when destory queue: " << GetHsaStatusString(status));
        return status;
    }

    // Trigger GPU event breakpoint
    TriggerGPUQueueUpdate();

    AGENT_LOG("Interception: Exit hsa_queue_destroy");

    return status;

}

static hsa_status_t
HsaDebugAgent_hsa_executable_freeze(
        hsa_executable_t executable,
        const char *options)
{
    AGENT_LOG("Interception: hsa_executable_freeze");

    hsa_status_t status = HSA_STATUS_SUCCESS;
    status = gs_OrigCoreApiTable.hsa_executable_freeze_fn(
            executable,
            options);
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot free executable");
        return status;
    }

    // Get loaded code object info of the freezed executable
    // and update _r_amd_gpu_debug
    status = AddExecCodeObjectInfo(executable);

    // Trigger GPU event breakpoint
    TriggerGPUCodeObjectUpdate();

    AGENT_LOG("Interception: Exit hsa_executable_freeze");

    return status;
}

static hsa_status_t
HsaDebugAgent_hsa_executable_destroy(
        hsa_executable_t executable)
{
    AGENT_LOG("Interception: hsa_executable_destroy");

    hsa_status_t status = HSA_STATUS_SUCCESS;

    // Remove loaded code object info of the deleted executable
    // and update _r_amd_gpu_debug
    status = DeleteExecCodeObjectInfo(executable);
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot delete code object info");
        return status;
    }

    status = gs_OrigCoreApiTable.hsa_executable_destroy_fn(
            executable);
    if (status != HSA_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot destroy executable");
        return status;
    }

    // Trigger GPU event breakpoint
    TriggerGPUCodeObjectUpdate();

    AGENT_LOG("Interception: Exit hsa_executable_destroy");

    return status;
}

static hsa_status_t
AddExecCodeObjectInfo(hsa_executable_t executable)
{
    hsa_status_t status = HSA_STATUS_SUCCESS;
    status = gs_OrigLoaderExtTable.
            hsa_ven_amd_loader_executable_iterate_loaded_code_objects(
                    executable,
                    AddCodeObjectInfoCallback,
                    NULL);
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

    CodeObjectInfo* pList = new CodeObjectInfo;
    pList->addrDelta = addrDelta;
    pList->addrLoaded = loadedBaseAddress;
    pList->sizeLoaded = loadedSize;
    pList->pPrev = nullptr;
    pList->pNext = nullptr;

    DebugAgentStatus agentStatus = DEBUG_AGENT_STATUS_SUCCESS;

    agentStatus = AddCodeObjectToList(pList);
    if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Interception: Cannot add code object info to link list");
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

static hsa_status_t
DeleteExecCodeObjectInfo(hsa_executable_t executable)
{
    hsa_status_t status = HSA_STATUS_SUCCESS;
    status = gs_OrigLoaderExtTable.
            hsa_ven_amd_loader_executable_iterate_loaded_code_objects(
                    executable,
                    DeleteCodeObjectInfoCallback,
                    NULL);
    return status;
}

static hsa_status_t
DeleteCodeObjectInfoCallback(
        hsa_executable_t executable,
        hsa_loaded_code_object_t loadedCodeObject,
        void *data)
{
    AGENT_LOG("Interception: DeleteCodeObjectInfoCallback");
    hsa_status_t status = HSA_STATUS_SUCCESS;

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

    RemoveCodeObjectFromList(loadedBaseAddress);

    AGENT_LOG("Interception: Exit DeleteCodeObjectInfoCallback");

    return status;
}

static hsa_status_t
HSADebugAgentHandleRuntimeEvent(const hsa_amd_event_t* event, void* pData)
{
    if (event == nullptr)
    {
        AGENT_ERROR("HSA Runtime provided a nullptr event pointer.");
        return HSA_STATUS_ERROR;
    }
    hsa_amd_event_t gpuEvent = *event;
    switch (gpuEvent.event_type)
    {
        case GPU_MEMORY_FAULT_EVENT :
            return HSADebugAgentHandleMemoryFault(gpuEvent, pData);
            break;
        default :
            return HSA_STATUS_SUCCESS;
    }
}
