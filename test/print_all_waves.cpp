#pragma clang optimize off
#include "util.h"
#include <hip/hip_runtime.h>

__global__ void
print_all_waves_kern ()
{
  if (threadIdx.x == 0)
    abort ();
  /* Make sure that by the time the debug agent prints all the waves following
     the abort, other waves in the workgroup are still present.  */
  __syncthreads ();
}

void
PrintAllWavesTest ()
{
  hipError_t err;
  print_all_waves_kern<<<2, 512>>> ();
  err = hipDeviceSynchronize ();
  TEST_ASSERT (err == hipSuccess, "kernel error");
}
