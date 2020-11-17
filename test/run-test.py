import os
import sys
import inspect
from subprocess import Popen, PIPE

# set up
if (len(sys.argv)  != 2):
    raise Exception("ERROR: Please specify test binary location. For example: $python3.6 run_test.py ./build")
else:
    test_binary_directory = sys.argv[1]
    print ("Test binary directory: ", os.path.abspath(test_binary_directory))
    agent_library_directory = os.path.abspath(test_binary_directory) + "/.."
    if not "LD_LIBRARY_PATH" in os.environ:
        os.environ["LD_LIBRARY_PATH"] = agent_library_directory
    else:
        os.environ["LD_LIBRARY_PATH"] += ":" + agent_library_directory
    os.environ["HSA_TOOLS_LIB"] = "librocm-debug-agent.so.2"
    os.chdir(test_binary_directory)
    # pre test to check if librocm-debug-agent.so.2 can be found
    p = Popen(['./rocm-debug-agent-test', '0'], stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    out_str = output.decode('utf-8')
    err_str = err.decode('utf-8')
    if (err_str):
        print (err_str)
        if ('\"librocm-debug-agent.so.2\" failed to load' in err_str):
            print("ERROR: Cannot find librocm-debug-agent.so.2, please set its location with environment variable LD_LIBRARY_PATH")
        sys.exit(1)

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
    check_list = ['Queue error (HSA_STATUS_ERROR_EXCEPTION: An HSAIL operation resulted in a hardware exception.)',
                  '(stopped, reason: ASSERT_TRAP)',
                   'exec: 0000000000000001',
#                  'status: 00012061',
#                  'trapsts: 20000000',
#                  'm0: 00000000',
                  's0:',
                  'v0:',
                  '0x0000: 22222222 11111111', # First uint64_t in LDS is '1111111122222222'
                  'Disassembly for function vector_add_assert_trap(int*, int*, int*)',
                  'vector_add_assert_trap.cpp:',
#                  '53          __builtin_trap ();', # Source files not always available (When install tests from package)
                  's_trap 2']
    p = Popen(['./rocm-debug-agent-test', '1'], stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    out_str = output.decode('utf-8')
    err_str = err.decode('utf-8')

    # check output string
    all_output_string_found = True
    for check_str in check_list:
        if (not (check_str in err_str)):
            all_output_string_found = False
            print ("\"", check_str, "\" Not Found in dump.")

    if (not all_output_string_found):
        print("rocm-debug-agent test print out.")
        print(out_str)
        print("rocm-debug-agent test error message.")
        print(err_str)


    return all_output_string_found

# test 2
def check_test_2():
    print("Starting rocm-debug-agent test 2")

    #TODO: use regular expressions instead of strings
    check_list = ['System event (HSA_AMD_GPU_MEMORY_FAULT_EVENT: page not present or supervisor privilege, write access to a read-only page)',
                  'Faulting page: 0x',
                  '(stopped, reason: MEMORY_VIOLATION)',
                  'exec: ffffffffffffffff',
#                  'status: 00012461',
#                  'trapsts: 30000100',
#                  'm0: 00001008',
                  's0:',
                  'v0:',
                  '0x0000: 22222222 11111111', # First uint64_t in LDS is '1111111122222222'
                  'Disassembly for function vector_add_memory_fault(int*, int*, int*)',
                  'vector_add_memory_fault.cpp:']
#                  'global_store_dword'] # Without precise memory, we can't guarantee that
    p = Popen(['./rocm-debug-agent-test', '2'], stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    out_str = output.decode('utf-8')
    err_str = err.decode('utf-8')

    # check output string
    all_output_string_found = True
    for check_str in check_list:
        if (not (check_str in err_str)):
            all_output_string_found = False
            print ("\"", check_str, "\" Not Found in dump.")

    if (not all_output_string_found):
        print("rocm-debug-agent test print out.")
        print(out_str)
        print("rocm-debug-agent test error message.")
        print(err_str)

    return all_output_string_found

test_success = True
test_success &= check_test_0()
test_success &= check_test_1()
test_success &= check_test_2()
if (test_success):
    print("rocm-debug-agent test Pass!")
else:
    raise Exception("rocm-debug-agent test fail!")
