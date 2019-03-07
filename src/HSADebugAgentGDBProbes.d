#!/usr/bin/dtrace -s

provider rocm_gdb_agent
{
    // A GPU user breakpoint has been hit.
    probe gpu_user_breakpoint ();

    // Agent initialization.
    probe init_start();
    probe init_complete(int status);

    // Agent finalization.
    probe fini_start();
    probe fini_complete(int status);

    // Executable loading/unloading.
    probe exec_load (ExecutableInfo* exec);
    probe exec_unload (ExecutableInfo* exec);

    // Queue created/destoryed.
    probe queue_create (QueueInfo* queue);
    probe queue_destroy (QueueInfo* queue);
};
