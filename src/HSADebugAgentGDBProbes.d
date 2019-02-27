#!/usr/bin/dtrace -s

provider rocr_debug_agent
{
    probe load_start ();
    probe load_complete ();
    probe unload_start ();
    probe unload_complete ();
    probe gpu_user_breakpoint ();
    probe executable_added (ExecutableInfo *executable);
    probe executable_removed (ExecutableInfo *executable);
    probe code_object_added (CodeObjectInfo *code_object);
    probe code_object_removed (CodeObjectInfo *code_object);
};
