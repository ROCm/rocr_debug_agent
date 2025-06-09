#include "util.h"
#include <hip/hip_runtime.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

__global__ void
sigquit_kern ()
{
  while (true)
    __builtin_amdgcn_s_sleep (16);
}

void
handle_alarm (int sig)
{
  fprintf (stderr, "Timeout reached. Exiting.\n");
  exit (EXIT_FAILURE);
}

void
SigquitTest ()
{
  signal (SIGALRM, handle_alarm);
  alarm (30);

  hipError_t err;
  sigquit_kern<<<1, 1>>> ();
  err = hipDeviceSynchronize ();

  alarm (0);

  TEST_ASSERT (err == hipSuccess, "kernel error");
}