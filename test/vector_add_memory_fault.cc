#include <string>
#include <string.h>
#include <signal.h>

#include "util.h"
#include "vector_add_memory_fault.h"

#define M_ORDER 64
#define M_GET(M, I, J) M[I * M_ORDER + J]
#define M_SET(M, I, J, V) M[I * M_ORDER + J] = V

static const uint32_t kNumBufferElements = 256;
static arguments *vectorAddKernArgs = NULL;
static const char CODE_OBJECT_NAME[] = "vector_add_memory_fault_kernel.o";
static const char KERNEL_NAME[] = "vector_add_memory_fault.kd";
static bool abort_called = false;
extern "C" void TestHandleAborts(int signal_number);

void VectorAddMemoryFaultTest(hsa_agent_t cpuAgent, hsa_agent_t gpuAgent) {
  hsa_status_t err;
  hsa_queue_t *queue = NULL;  // command queue
  hsa_signal_t completion_signal = {0};  // completion signal

  int *M_IN0 = NULL;
  int *M_IN1 = NULL;
  int *M_RESULT_DEVICE = NULL;
  int M_RESULT_HOST[M_ORDER * M_ORDER];

  // get queue size
  uint32_t queue_size = 0;
  err = hsa_agent_get_info(gpuAgent,
                           HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_agent_get_info");

  // create queue
  err = hsa_queue_create(gpuAgent,
                         queue_size, HSA_QUEUE_TYPE_MULTI,
                         NULL, NULL, 0, 0, &queue);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_queue_create");

  // Find a memory pool that supports kernel arguments.
  hsa_amd_memory_pool_t kernarg_pool;
  err = hsa_amd_agent_iterate_memory_pools(cpuAgent,
                                           GetKernArgMemoryPool,
                                           &kernarg_pool);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_amd_agent_iterate_memory_pools");

  // Get System Memory Pool on the cpuAgent to allocate host side buffers
  hsa_amd_memory_pool_t global_pool;
  err = hsa_amd_agent_iterate_memory_pools(cpuAgent,
                                           GetGlobalMemoryPool,
                                           &global_pool);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_amd_agent_iterate_memory_pools");

  // allocate input and output kernel arguments
  err = hsa_amd_memory_pool_allocate(global_pool,
                                     M_ORDER * M_ORDER * sizeof(int), 0,
                                     reinterpret_cast<void**>(&M_IN0));
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_amd_memory_pool_allocate");

  err = hsa_amd_memory_pool_allocate(global_pool,
                                     M_ORDER * M_ORDER * sizeof(int), 0,
                                     reinterpret_cast<void**>(&M_IN1));
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_amd_memory_pool_allocate");

  err = hsa_amd_memory_pool_allocate(global_pool,
                                     M_ORDER * M_ORDER * sizeof(int), 0,
                                     reinterpret_cast<void**>(&M_RESULT_DEVICE));
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_amd_memory_pool_allocate");

  // create kernel arguments
  err = hsa_amd_memory_pool_allocate(kernarg_pool,
                                     sizeof(arguments), 0,
                                     reinterpret_cast<void**>(&vectorAddKernArgs));
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_amd_memory_pool_allocate");

  // Allow gpuAgent access to all allocated system memory.
  err = hsa_amd_agents_allow_access(1, &gpuAgent, NULL, M_IN0);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_amd_agents_allow_access");
  err = hsa_amd_agents_allow_access(1, &gpuAgent, NULL, M_IN1);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_amd_agents_allow_access");
  err = hsa_amd_agents_allow_access(1, &gpuAgent, NULL, M_RESULT_DEVICE);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_amd_agents_allow_access");
  err = hsa_amd_agents_allow_access(1, &gpuAgent, NULL, vectorAddKernArgs);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_amd_agents_allow_access");

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

  std::string kernel_file = isaName + "/" + CODE_OBJECT_NAME;
  hsa_file_t file_handle = open(kernel_file.c_str(), O_RDONLY);
  TEST_ASSERT(file_handle != -1, "Cannot open kernel file");

  err = hsa_code_object_reader_create_from_file(file_handle, &code_obj_rdr);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_code_object_reader_create_from_file");

  err = hsa_executable_create_alt(HSA_PROFILE_FULL,
                                  HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT,
                                                          NULL, &executable);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_executable_create_alt");

  err = hsa_executable_load_agent_code_object(executable, gpuAgent, code_obj_rdr,
        NULL, NULL);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_executable_load_agent_code_object");

  err = hsa_executable_freeze(executable, NULL);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_executable_freeze");

  hsa_executable_symbol_t kern_sym;
  err = hsa_executable_get_symbol(executable, NULL, KERNEL_NAME, gpuAgent,
                                  0, &kern_sym);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_executable_get_symbol");

  uint64_t codeHandle;
  err = hsa_executable_symbol_get_info(kern_sym,
                       HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &codeHandle);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_executable_symbol_get_info");

  // set abort handler for  test.
  signal(SIGABRT, &TestHandleAborts);

  // Fill the dispatch packet with
  // workgroup_size, grid_size, kernelArgs and completion signal
  // Put it on the queue and launch the kernel by ringing the doorbell

  // create completion signal
  err = hsa_signal_create(1, 0, NULL, &completion_signal);
  TEST_ASSERT(err == HSA_STATUS_SUCCESS, "hsa_signal_create");

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
  aql.group_segment_size = 4;
  aql.kernel_object = codeHandle;  // kernel_code;
  aql.kernarg_address = vectorAddKernArgs;
  aql.completion_signal = completion_signal;

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
  completion = hsa_signal_wait_scacquire(completion_signal, HSA_SIGNAL_CONDITION_LT, 1,
                                         0xffffffff, HSA_WAIT_STATE_ACTIVE);
  if (completion != 1)
    std::cerr << "completion signal error" << std::endl;
}

// Only should be call at the end of program.
void TestHandleAborts(int signal_number)
{
  std::cout<< "rocm-debug-agent abort() as expected" << std::endl;
  abort_called = true;
}

