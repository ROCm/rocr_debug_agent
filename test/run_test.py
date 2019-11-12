import os
import sys
import inspect
from subprocess import Popen, PIPE

# set up
if (len(sys.argv)  != 2):
    raise Exception("rocm-debug-agent run_test input error!")
else:
    test_binary_directory = sys.argv[1]
    print ("Test binary directory: ", os.path.abspath(test_binary_directory))
    print ("librocm-debug-agent.so directroy: ", os.environ["LD_LIBRARY_PATH"])
    os.environ["HSA_TOOLS_LIB"] = "librocm-debug-agent.so"
    os.chdir(test_binary_directory)

# test 0
def check_test_0():
    print("Starting rocm-debug-agent-test 0")
    p = Popen(['./rocm-debug-agent-test', '0'], stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    out_str = output.decode('utf-8')
    err_str = err.decode('utf-8')

# Only print but not throw for err_str, since debug build has print out could be ignored
    if (err_str):
        print (err_str)

    return True

# test 1
def check_test_1():
    print("Starting rocm-debug-agent test 1")

    #TODO: use regular expressions instead of strings
    check_list = ['Queue error state in GPU agent: AMD gfx',
                  'Node:',
                  'Queue ID:',
                  '(Debug trap;)',
                  'runtime catched trap instruction successfully',
                  'custom queue error handler completed successfully',
#                  'EXEC: 0xFFFFFFFFFFFFFFFF',
#                  'STATUS: 0x00012061',
#                  'TRAPSTS: 0x30000100',
#                  'M0: 0x80000000',
                  's0:',
                  'v0:',
                  '0x00000002', # one of sgprs should be '2'
                  '0x0000:  0x00000001',  #First uint32_t in LDS is '1'
                  'vector_add_debug_trap_kernel.cl:10',
                  '__builtin_trap();',
                  's_trap 2']
    p = Popen(['./rocm-debug-agent-test', '1'], stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    out_str = output.decode('utf-8')
    err_str = err.decode('utf-8')

    # check output string
    all_output_string_found = True
    for check_str in check_list:
        if (not (check_str in out_str)):
            all_output_string_found = False
            print ("\"", check_str, "\" Not Found in dump.")

    if (err_str):
        print (err_str)

    return all_output_string_found

# test 2
def check_test_2():
    print("Starting rocm-debug-agent test 2")

    #TODO: use regular expressions instead of strings
    check_list = ['Memory access fault at GPU Node:',
                  'rocm-debug-agent abort() as expected',
                  'Address:',
#                  '(page not present;write access to a read-only page;)',
#                  'EXEC: 0xFFFFFFFFFFFFFFFF',
#                  'STATUS: 0x00012460',
#                  'TRAPSTS: 0x30000100',
#                  'M0: 0x80000000',
                  's0:',
                  'v0:',
                  '0x00000002', # one of sgprs should be '2'
                  '0x0000:  0x00000001', #First uint32_t in LDS is '1'
                  'vector_add_memory_fault_kernel.cl:10',
                  'global_store_dword']
    p = Popen(['./rocm-debug-agent-test', '2'], stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    out_str = output.decode('utf-8')
    err_str = err.decode('utf-8')

    # check output string
    all_output_string_found = True
    for check_str in check_list:
        if (not (check_str in out_str)):
            all_output_string_found = False
            print ("\"", check_str, "\" Not Found in dump.")

    if (err_str):
        print (err_str)

    return all_output_string_found

test_success = True
test_success &= check_test_0()
test_success &= check_test_1()
test_success &= check_test_2()
if (test_success):
    print("rocm-debug-agent test Pass!")
else:
    raise Exception("rocm-debug-agent test fail!")
