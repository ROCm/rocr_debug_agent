#include <algorithm>
#include <assert.h>
#include <fcntl.h>
#include <hsa.h>
#include <hsa_ext_amd.h>
#include <iostream>
#include <memory>
#include <string.h>
#include <vector>

#define M_ORDER 64
#define M_GET(M, I, J) M[I * M_ORDER + J]
#define M_SET(M, I, J, V) M[I * M_ORDER + J] = V

static const uint32_t kNumBufferElements = 256;
typedef struct test_debug_data_t {
  bool trap_triggered;
  hsa_queue_t** queue_pointer;
} test_debug_data;


static void TestDebugTrap(hsa_status_t status, hsa_queue_t *source, void *data);
static void VectorAddDebugTrapTest(hsa_agent_t cpuAgent, hsa_agent_t gpuAgent);
static hsa_status_t GetGlobalMemoryPool(hsa_amd_memory_pool_t pool, void* data);
static hsa_status_t GetKernArgMemoryPool(hsa_amd_memory_pool_t pool, void* data);
static hsa_status_t IterateCPUAgents(hsa_agent_t agent, void *data);
static hsa_status_t IterateGPUAgents(hsa_agent_t agent, void *data);
static hsa_status_t QueryAgentISACallback(hsa_isa_t isa, void *data);
static void WriteAQLToQueue(hsa_kernel_dispatch_packet_t const* in_aql, hsa_queue_t* q);

// This wrapper atomically writes the provided header and setup to the
// provided AQL packet. The provided AQL packet address should be in the
// queue memory space.
static inline void AtomicSetPacketHeader(uint16_t header, uint16_t setup,
                                  hsa_kernel_dispatch_packet_t* queue_packet) {
  __atomic_store_n(reinterpret_cast<uint32_t*>(queue_packet),
                   header | (setup << 16), __ATOMIC_RELEASE);
}

typedef struct __attribute__((aligned(16))) arguments_t {
  const int *a;
  const int *b;
  const int *c;
  int *d;
  int *e;
} arguments;

arguments *vectorAddKernArgs = NULL;

static const char CODE_OBJECT_NAME[] = "vector_add_debug_trap_kernel.o";
static const char KERNEL_NAME[] = "vector_add_debug_trap";
static const char TEST_SEPARATOR[] = "  **************************";

static void PrintDebugSubtestHeader(const char *header) {
  std::cout << "  *** Debug Agent Test: " << header << " ***" << std::endl;
}

static void WriteAQLToQueue(hsa_kernel_dispatch_packet_t const* in_aql,
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

static void VectorAddDebugTrapTest(hsa_agent_t cpuAgent, hsa_agent_t gpuAgent) {
  hsa_status_t err;
  hsa_queue_t *queue = NULL;  // command queue
  hsa_signal_t signal = {0};  // completion signal

  int *M_IN0 = NULL;
  int *M_IN1 = NULL;
  int *M_RESULT_DEVICE = NULL;
  int M_RESULT_HOST[M_ORDER * M_ORDER];

  // get queue size
  uint32_t queue_size = 0;
  err = hsa_agent_get_info(gpuAgent,
                           HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size);
  assert(err == HSA_STATUS_SUCCESS);

  test_debug_data user_data{.trap_triggered = false,
                            .queue_pointer = &queue};

  // create queue
  err = hsa_queue_create(gpuAgent,
                         queue_size, HSA_QUEUE_TYPE_MULTI,
                         TestDebugTrap, &user_data, 0, 0, &queue);
  assert(err == HSA_STATUS_SUCCESS);

  // Find a memory pool that supports kernel arguments.
  hsa_amd_memory_pool_t kernarg_pool;
  err = hsa_amd_agent_iterate_memory_pools(cpuAgent,
                                           GetKernArgMemoryPool,
                                           &kernarg_pool);
  assert(err == HSA_STATUS_SUCCESS);

  // Get System Memory Pool on the cpuAgent to allocate host side buffers
  hsa_amd_memory_pool_t global_pool;
  err = hsa_amd_agent_iterate_memory_pools(cpuAgent,
                                           GetGlobalMemoryPool,
                                           &global_pool);
  assert(err == HSA_STATUS_SUCCESS);

  // allocate input and output kernel arguments
  err = hsa_amd_memory_pool_allocate(global_pool,
                                     M_ORDER * M_ORDER * sizeof(int), 0,
                                     reinterpret_cast<void**>(&M_IN0));
  assert(err == HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_allocate(global_pool,
                                     M_ORDER * M_ORDER * sizeof(int), 0,
                                     reinterpret_cast<void**>(&M_IN1));
  assert(err == HSA_STATUS_SUCCESS);

  err = hsa_amd_memory_pool_allocate(global_pool,
                                     M_ORDER * M_ORDER * sizeof(int), 0,
                                     reinterpret_cast<void**>(&M_RESULT_DEVICE));
  assert(err == HSA_STATUS_SUCCESS);

  // create kernel arguments
  err = hsa_amd_memory_pool_allocate(kernarg_pool,
                                     sizeof(arguments), 0,
                                     reinterpret_cast<void**>(&vectorAddKernArgs));
  assert(err == HSA_STATUS_SUCCESS);

  // Allow gpuAgent access to all allocated system memory.
  err = hsa_amd_agents_allow_access(1, &gpuAgent, NULL, M_IN0);
  assert(err == HSA_STATUS_SUCCESS);
  err = hsa_amd_agents_allow_access(1, &gpuAgent, NULL, M_IN1);
  assert(err == HSA_STATUS_SUCCESS);
  err = hsa_amd_agents_allow_access(1, &gpuAgent, NULL, M_RESULT_DEVICE);
  assert(err == HSA_STATUS_SUCCESS);
  err = hsa_amd_agents_allow_access(1, &gpuAgent, NULL, vectorAddKernArgs);
  assert(err == HSA_STATUS_SUCCESS);

  memset(M_RESULT_HOST, 0, M_ORDER * M_ORDER * sizeof(int));
  memset(M_RESULT_DEVICE, 0, M_ORDER * M_ORDER * sizeof(int));

  vectorAddKernArgs->a = M_IN0;
  vectorAddKernArgs->b = M_IN1;
  vectorAddKernArgs->c = M_RESULT_DEVICE;

  // initialize input and run on host
  srand(time(NULL));
  for (int i = 0; i < M_ORDER; ++i) {
    for (int j = 0; j < M_ORDER; ++j) {
      M_SET(M_IN0, i, j, (1 + rand() % 10));
      M_SET(M_IN1, i, j, (1 + rand() % 10));
    }
  }

  for (int i = 0; i < M_ORDER; ++i) {
    for (int j = 0; j < M_ORDER; ++j) {
      int s = M_GET(M_IN0, i, j) + M_GET(M_IN1, i, j);
      M_SET(M_RESULT_HOST, i, j, s);
    }
  }

  // Create the executable, get symbol by name and load the code object
  hsa_code_object_reader_t code_obj_rdr = {0};
  hsa_executable_t executable = {0};

  hsa_file_t file_handle = open(CODE_OBJECT_NAME, O_RDONLY);
  assert(file_handle != -1);

  err = hsa_code_object_reader_create_from_file(file_handle, &code_obj_rdr);
  assert(err == HSA_STATUS_SUCCESS);

  err = hsa_executable_create_alt(HSA_PROFILE_FULL,
                                  HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT,
                                                          NULL, &executable);
  assert(err == HSA_STATUS_SUCCESS);

  err = hsa_executable_load_agent_code_object(executable, gpuAgent, code_obj_rdr,
        NULL, NULL);
  assert(err == HSA_STATUS_SUCCESS);

  err = hsa_executable_freeze(executable, NULL);
  assert(err == HSA_STATUS_SUCCESS);

  hsa_executable_symbol_t kern_sym;
  err = hsa_executable_get_symbol(executable, NULL, KERNEL_NAME, gpuAgent,
                                  0, &kern_sym);
  assert(err == HSA_STATUS_SUCCESS);

  uint64_t codeHandle;
  err = hsa_executable_symbol_get_info(kern_sym,
                       HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &codeHandle);
  assert(err == HSA_STATUS_SUCCESS);
  // Fill the dispatch packet with
  // workgroup_size, grid_size, kernelArgs and completion signal
  // Put it on the queue and launch the kernel by ringing the doorbell

  // create completion signal
  err = hsa_signal_create(1, 0, NULL, &signal);
  assert(err == HSA_STATUS_SUCCESS);

  // create aql packet
  hsa_kernel_dispatch_packet_t aql;
  memset(&aql, 0, sizeof(aql));

  // initialize aql packet
  aql.header = 0;
  aql.setup = 1;
  aql.workgroup_size_x = 64;
  aql.workgroup_size_y = 1;
  aql.workgroup_size_z = 1;
  aql.grid_size_x = M_ORDER * M_ORDER;
  aql.grid_size_y = 1;
  aql.grid_size_z = 1;
  aql.private_segment_size = 0;
  aql.group_segment_size = 0;
  aql.kernel_object = codeHandle;  // kernel_code;
  aql.kernarg_address = vectorAddKernArgs;
  aql.completion_signal = signal;

  // const uint32_t queue_size = queue->size;
  const uint32_t queue_mask = queue->size - 1;

  // write to command queue
  uint64_t index = hsa_queue_load_write_index_relaxed(queue);

  // This function simply copies the data we've collected so far into our
  // local AQL packet, except the the setup and header fields.
  WriteAQLToQueue(&aql, queue);

  uint32_t aql_header = HSA_PACKET_TYPE_KERNEL_DISPATCH;
  aql_header |= HSA_FENCE_SCOPE_SYSTEM <<
                HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE;
  aql_header |= HSA_FENCE_SCOPE_SYSTEM <<
                HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE;

  void* q_base = queue->base_address;
  AtomicSetPacketHeader(aql_header, aql.setup,
                        &(reinterpret_cast<hsa_kernel_dispatch_packet_t*>
                            (q_base))[index & queue_mask]);

  hsa_queue_store_write_index_relaxed(queue, index + 1);

  // ringdoor bell
  hsa_signal_store_relaxed(queue->doorbell_signal, index);

  // wait for the signal long enough for the debug trap event to happen
  hsa_signal_value_t completion;
  completion = hsa_signal_wait_scacquire(signal, HSA_SIGNAL_CONDITION_LT, 1,
                                         0xffffffff, HSA_WAIT_STATE_ACTIVE);

  // completion signal should not be changed.
  assert(completion == 1);

  // trap should be triggered
  assert(user_data.trap_triggered == true);

  hsa_signal_store_relaxed(signal, 1);

  if (M_IN0) { hsa_memory_free(M_IN0); }
  if (M_IN1) { hsa_memory_free(M_IN1); }
  if (M_RESULT_DEVICE) {hsa_memory_free(M_RESULT_DEVICE); }
  if (vectorAddKernArgs) { hsa_memory_free(vectorAddKernArgs); }
  if (signal.handle) { hsa_signal_destroy(signal); }
  if (queue) { hsa_queue_destroy(queue); }
  std::cout << TEST_SEPARATOR << std::endl;
}

int main() {
  hsa_status_t err;

  // initiale hsa
  err = hsa_init();
  assert(err == HSA_STATUS_SUCCESS);

  PrintDebugSubtestHeader("VectorAddDebugTrapTest");

  // find all cpu agents
  std::vector<hsa_agent_t> cpus;
  err = hsa_iterate_agents(IterateCPUAgents, &cpus);
  assert(err == HSA_STATUS_SUCCESS);

  // find all gpu agents
  std::vector<hsa_agent_t> gpus;
  err = hsa_iterate_agents(IterateGPUAgents, &gpus);
  assert(err == HSA_STATUS_SUCCESS);

  if (gpus.size() == 0){
    std::cout << "No supported GPU found, exit test." << std::endl;
  }
  else {
    for (unsigned int i = 0 ; i< gpus.size(); ++i) {
      VectorAddDebugTrapTest(cpus[0], gpus[i]);
    }
  }

  std::cout << "Test Finished" << std::endl;
  std::cout << TEST_SEPARATOR << std::endl;
}

static void TestDebugTrap(hsa_status_t status, hsa_queue_t *source, void *data) {
  std::cout<< "runtime catched trap instruction successfully"<< std::endl;
  assert(source != NULL);
  assert(data != NULL);

  test_debug_data *debug_data = reinterpret_cast<test_debug_data*>(data);
  hsa_queue_t * queue  = *(debug_data->queue_pointer);
  debug_data->trap_triggered = true;
  // check the status
  assert(status == HSA_STATUS_ERROR_EXCEPTION);

  // check the queue id and user data
  assert(source->id == queue->id);
  std::cout<< "custom queue error handler completed successfully"<< std::endl;
}

// Find  a memory pool that can be used for kernarg locations.
static hsa_status_t GetKernArgMemoryPool(hsa_amd_memory_pool_t pool, void* data) {
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
static hsa_status_t GetGlobalMemoryPool(hsa_amd_memory_pool_t pool, void* data) {
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
static hsa_status_t IterateCPUAgents(hsa_agent_t agent, void *data) {
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
static hsa_status_t IterateGPUAgents(hsa_agent_t agent, void *data) {
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
    bool supGFX900 = false;
    status = hsa_agent_iterate_isas(
        agent, QueryAgentISACallback, &supGFX900);
    assert(status == HSA_STATUS_SUCCESS);
    if (HSA_STATUS_SUCCESS == status && supGFX900 ==true)
      gpus->push_back(agent);
  }
  return status;
}

static hsa_status_t QueryAgentISACallback(hsa_isa_t isa, void *data)
{
  if (data == nullptr){
    return HSA_STATUS_ERROR;
  }

  const char gfx900[] = "amdgcn-amd-amdhsa--gfx900";
  char isaName[64];

  //TODO: check isa name length
  hsa_status_t status = hsa_isa_get_info_alt(
     isa, HSA_ISA_INFO_NAME, isaName);
  if (strcmp(isaName, gfx900) == 0){
    *(bool*)data = true;
  }
  return status;
}
