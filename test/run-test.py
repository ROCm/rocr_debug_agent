import os
import re
import sys
import inspect
from subprocess import Popen, PIPE


def filter_warnings(err_str):
    """ Filter out warnings wich are expected on some archs.  """
    return "\n".join([
        line for line in err_str.split("\n")
        if not ("Precise memory not supported for all the agents" in line
                or "architecture not supported" in line
                or "Warning: Resource leak detected" in line)
    ])


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
    os.environ["ROCM_DEBUG_AGENT_OPTIONS"] = "-p"
    os.chdir(test_binary_directory)
    # pre test to check if librocm-debug-agent.so.2 can be found
    p = Popen(['./rocm-debug-agent-test', '0'], stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    out_str = output.decode('utf-8')
    err_str = err.decode('utf-8')

    if (filter_warnings(err_str)):
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
    if (filter_warnings(err_str)):
        print (err_str)

    return True

# test 1
def check_test_1():
    print("Starting rocm-debug-agent test 1")

    #TODO: use regular expressions instead of strings
    check_list = ['HSA_STATUS_ERROR_EXCEPTION: An HSAIL operation resulted in a hardware exception\\.',
                  '\\(stopped, reason: ASSERT_TRAP\\)',
                   'exec: (00000000)?00000001',
#                  'status: 00012061',
#                  'trapsts: 20000000',
#                  'm0: 00000000',
                  's0:',
                  'v0:',
                  '0x0000: 22222222 11111111', # First uint64_t in LDS is '1111111122222222'
                  'Disassembly for function vector_add_assert_trap\\(int\\*, int\\*, int\\*\\)'
#                  'vector_add_assert_trap.cpp:', # Debug info may not be available on some older distributions
#                  '53          __builtin_trap ();', # Source files not always available (When install tests from package)
#                  's_trap 2'
                  ]

    p = Popen(['./rocm-debug-agent-test', '1'], stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    out_str = output.decode('utf-8')
    err_str = err.decode('utf-8')

    # check output string
    all_output_string_found = True
    for check_str in check_list:
        pattern = re.compile(check_str)
        if (not (pattern.search(err_str))):
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
    check_list = [
#                  'System event \(HSA_AMD_GPU_MEMORY_FAULT_EVENT\)',
#                  'Faulting page: 0x',
                  '\\(stopped, reason: MEMORY_VIOLATION\\)',
                  'exec: (ffffffff)?ffffffff',
#                  'status: 00012461',
#                  'trapsts: 30000100',
#                  'm0: 00001008',
                  's0:',
                  'v0:',
                  '0x0000: 22222222 11111111', # First uint64_t in LDS is '1111111122222222'
                  'Disassembly for function vector_add_memory_fault\\(int\\*, int\\*, int\\*\\)'
#                  'vector_add_memory_fault.cpp:', Debug info may not be available on some older distributions
#                  'global_store_dword' # Without precise memory, we can't guarantee that
                  ]
    p = Popen(['./rocm-debug-agent-test', '2'], stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    out_str = output.decode('utf-8')
    err_str = err.decode('utf-8')

    # check output string
    all_output_string_found = True
    for check_str in check_list:
        pattern = re.compile(check_str)
        if (not (pattern.search(err_str))):
            all_output_string_found = False
            print ("\"", check_str, "\" Not Found in dump.")

    if (not all_output_string_found):
        print("rocm-debug-agent test print out.")
        print(out_str)
        print("rocm-debug-agent test error message.")
        print(err_str)

    return all_output_string_found

# test 3: snapshot code object on load
def check_test_3():
    print("Starting rocm-debug-agent test 3")

    p = Popen(['./rocm-debug-agent-test', '3'], stdout=PIPE, stderr=PIPE)
    output, err = p.communicate()
    out_str = output.decode('utf-8')
    err_str = err.decode('utf-8')

    found_error = False

    # If the debug agent did not capture the code object on load, it should
    # not be able to open it on exception, leading to the following warning:
    #
    #    rocm-debug-agent: warning: elf_getphdrnum failed for `memory://226967#offset=0x1651d4f0&size=3456'
    #    rocm-debug-agent: warning: could not open code_object_1
    if "could not open code_object" in  err_str:
        found_error = True

    # If the code object was not properly loaded, we should not have any
    # disassembly in the output
    if ("Disassembly:\n" not in err_str
            and "Disassembly for function kernel_abort:\n" not in err_str):
        found_error = True

    if (found_error):
        print("rocm-debug-agent test print out.")
        print(out_str)
        print("rocm-debug-agent test error message.")
        print(err_str)

    return not found_error

test_success = True

for deferred_loading in (False, True):
    if deferred_loading:
        print("### Testing with HIP_ENABLE_DEFERRED_LOADING=1")
        os.environ["HIP_ENABLE_DEFERRED_LOADING"] = "1"
    else:
        print("### Testing without HIP_ENABLE_DEFERRED_LOADING")
        if "HIP_ENABLE_DEFERRED_LOADING" in os.environ:
            del os.environ["HIP_ENABLE_DEFERRED_LOADING"]

    test_success &= check_test_0()
    test_success &= check_test_1()
    test_success &= check_test_2()
    test_success &= check_test_3()

if (test_success):
    print("rocm-debug-agent test Pass!")
else:
    raise Exception("rocm-debug-agent test fail!")
