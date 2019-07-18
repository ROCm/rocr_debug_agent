#include <string>
#include <string.h>
#include "util.h"

void WriteAQLToQueue(hsa_kernel_dispatch_packet_t const* in_aql,
                            hsa_queue_t* q) {
  void* queue_base = q->base_address;
  const uint32_t queue_mask = q->size - 1;
  uint64_t que_idx = hsa_queue_add_write_index_relaxed(q, 1);

  hsa_kernel_dispatch_packet_t* queue_aql_packet;

  queue_aql_packet =
    &(reinterpret_cast<hsa_kernel_dispatch_packet_t*>(queue_base))
    [que_idx & queue_mask];

  queue_aql_packet->workgroup_size_x = in_aql->workgroup_size_x;
  queue_aql_packet->workgroup_size_y = in_aql->workgroup_size_y;
  queue_aql_packet->workgroup_size_z = in_aql->workgroup_size_z;
  queue_aql_packet->grid_size_x = in_aql->grid_size_x;
  queue_aql_packet->grid_size_y = in_aql->grid_size_y;
  queue_aql_packet->grid_size_z = in_aql->grid_size_z;
  queue_aql_packet->private_segment_size = in_aql->private_segment_size;
  queue_aql_packet->group_segment_size = in_aql->group_segment_size;
  queue_aql_packet->kernel_object = in_aql->kernel_object;
  queue_aql_packet->kernarg_address = in_aql->kernarg_address;
  queue_aql_packet->completion_signal = in_aql->completion_signal;
}

// Find  a memory pool that can be used for kernarg locations.
hsa_status_t GetKernArgMemoryPool(hsa_amd_memory_pool_t pool, void* data) {
  hsa_status_t err;
  if (data == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  hsa_amd_segment_t segment;
  err = hsa_amd_memory_pool_get_info(pool,
                                         HSA_AMD_MEMORY_POOL_INFO_SEGMENT,
                                         &segment);
  assert(err == HSA_STATUS_SUCCESS);
  if (HSA_AMD_SEGMENT_GLOBAL != segment) {
    return HSA_STATUS_SUCCESS;
  }

  hsa_amd_memory_pool_global_flag_t flags;
  err = hsa_amd_memory_pool_get_info(pool,
                                         HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS,
                                         &flags);
  assert(err == HSA_STATUS_SUCCESS);

  if (flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_KERNARG_INIT) {
    hsa_amd_memory_pool_t* ret =
                                reinterpret_cast<hsa_amd_memory_pool_t*>(data);
    *ret = pool;
  }

  return HSA_STATUS_SUCCESS;
}

// Find coarse grained system memory.
hsa_status_t GetGlobalMemoryPool(hsa_amd_memory_pool_t pool, void* data) {
  hsa_amd_segment_t segment;
  hsa_status_t err;
  err = hsa_amd_memory_pool_get_info(pool,
                                         HSA_AMD_MEMORY_POOL_INFO_SEGMENT,
                                         &segment);
  if (HSA_AMD_SEGMENT_GLOBAL != segment)
    return err;

  hsa_amd_memory_pool_global_flag_t flags;
  err = hsa_amd_memory_pool_get_info(pool,
                                        HSA_AMD_MEMORY_POOL_INFO_GLOBAL_FLAGS,
                                        &flags);
  assert(err == HSA_STATUS_SUCCESS);

  // this is valid for dGPUs. But on APUs, it has to be FINE_GRAINED
  if (flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_COARSE_GRAINED) {
    hsa_amd_memory_pool_t* ret =
                                reinterpret_cast<hsa_amd_memory_pool_t*>(data);
    *ret = pool;
  } else {  // this is for APUs
    if (flags & HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED) {
      hsa_amd_memory_pool_t* ret =
                                reinterpret_cast<hsa_amd_memory_pool_t*>(data);
      *ret = pool;
    }
  }
  return err;
}

// Find CPU Agents
hsa_status_t IterateCPUAgents(hsa_agent_t agent, void *data) {
  hsa_status_t status;
  assert(data != NULL);
  if (data == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  std::vector<hsa_agent_t>* cpus = static_cast<std::vector<hsa_agent_t>*>(data);
  hsa_device_type_t device_type;
  status = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
  assert(status == HSA_STATUS_SUCCESS);
  if (HSA_STATUS_SUCCESS == status && HSA_DEVICE_TYPE_CPU == device_type) {
    cpus->push_back(agent);
  }
  return status;
}

// Find GPU Agents
hsa_status_t IterateGPUAgents(hsa_agent_t agent, void *data) {
  hsa_status_t status;
  assert(data != NULL);
  if (data == NULL) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }
  std::vector<hsa_agent_t>* gpus = static_cast<std::vector<hsa_agent_t>*>(data);
  hsa_device_type_t device_type;
  status = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
  assert(status == HSA_STATUS_SUCCESS);

  if (HSA_STATUS_SUCCESS == status && HSA_DEVICE_TYPE_GPU == device_type) {
    bool supGFX = false;
    status = hsa_agent_iterate_isas(
        agent, QueryAgentISACallback, &supGFX);
    assert(status == HSA_STATUS_SUCCESS);
    if (HSA_STATUS_SUCCESS == status && supGFX ==true)
      gpus->push_back(agent);
  }
  return status;
}

hsa_status_t QueryAgentISACallback(hsa_isa_t isa, void *data)
{
  if (data == nullptr){
    return HSA_STATUS_ERROR;
  }

  const char gfx900[] = "amdgcn-amd-amdhsa--gfx900";
  const char gfx906[] = "amdgcn-amd-amdhsa--gfx906";
  const char gfx908[] = "amdgcn-amd-amdhsa--gfx908";
  char isaNameTmp[64];

  //TODO: check isa name length
  hsa_status_t status = hsa_isa_get_info_alt(
     isa, HSA_ISA_INFO_NAME, isaNameTmp);
  if (strcmp(isaNameTmp, gfx900) == 0) {
    isaName = "gfx900";
    *(bool*)data = true;
  }
  else if (strcmp(isaNameTmp, gfx906) == 0) {
    isaName = "gfx906";
    *(bool*)data = true;
  }
  else if (strcmp(isaNameTmp, gfx908) == 0) {
    isaName = "gfx908";
    *(bool*)data = true;
  }
  return status;
}
