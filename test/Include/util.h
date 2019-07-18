#ifndef UTIL_H_
#define UTIL_H_

#include <algorithm>
#include <assert.h>
#include <fcntl.h>
#include <hsa.h>
#include <hsa_ext_amd.h>
#include <iostream>
#include <memory>
#include <vector>

typedef struct __attribute__((aligned(16))) arguments_t {
  const int *a;
  const int *b;
  const int *c;
  int *d;
  int *e;
} arguments;

const char TEST_SEPARATOR[] = "  **************************";
extern std::string isaName;

hsa_status_t GetGlobalMemoryPool(hsa_amd_memory_pool_t pool, void* data);
hsa_status_t GetKernArgMemoryPool(hsa_amd_memory_pool_t pool, void* data);
hsa_status_t IterateCPUAgents(hsa_agent_t agent, void *data);
hsa_status_t IterateGPUAgents(hsa_agent_t agent, void *data);
hsa_status_t QueryAgentISACallback(hsa_isa_t isa, void *data);
void WriteAQLToQueue(hsa_kernel_dispatch_packet_t const* in_aql, hsa_queue_t* q);

// This wrapper atomically writes the provided header and setup to the
// provided AQL packet. The provided AQL packet address should be in the
// queue memory space.
inline void AtomicSetPacketHeader(uint16_t header, uint16_t setup,
                                  hsa_kernel_dispatch_packet_t* queue_packet) {
  __atomic_store_n(reinterpret_cast<uint32_t*>(queue_packet),
                   header | (setup << 16), __ATOMIC_RELEASE);
}
#endif // UTIL_H_
