#!/usr/bin/dtrace -s

provider rocr_debug_agent
{
    probe executable_added (ExecutableInfo *executable);
    probe executable_removed (ExecutableInfo *executable);
    probe code_object_added (CodeObjectInfo *code_object);
    probe code_object_removed (CodeObjectInfo *code_object);
};
