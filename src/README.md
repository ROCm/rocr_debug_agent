# Building the rocr_debug_agent

The rocr_debug_agent can be built on Ubuntu 16.04, Ubuntu 18.04, and Centos 7.6.

To build the rocr_debug_agent, compatible versions of the runtime and the
libhsakmt library must be available. The latest versions of these files are
available from the ROCR-Runtime and ROCT-Thunk-Interface repositories, here:

* ROCR-Runtime https://github.com/RadeonOpenCompute/ROCR-Runtime
* ROCT-Thunk-Interface: https://github.com/RadeonOpenCompute/ROCT-Thunk-Interface

Build the rocr_debug_agent Library

1. Specify the ROCR-Runtime headers and libraries path, the ROCT-Thunk-Interface
   headers and libraries path, and the lightning compiler path in Debug_Agent_PREFIX_PATH
   if they are not in the default "/opt/rocm/" location.
2. Make a new folder 'build' under the src folder
3. Cd into 'build', run CMAKE to generate the makefiles, and build

For example, from the 'src' directory execute:

````
  export Debug_Agent_PREFIX_PATH=<Path to ROC Runtime headers>:\
                                 <Path to ROC Runtime libraries>:\
                                 <Path to ROC Thunk headers>:\
                                 <Path to ROC Thunk libraries>:\
                                 <Path to Lightning Compiler Clang>
  mkdir build
  cd build
  cmake -DCMAKE_PREFIX_PATH=$Debug_Agent_PREFIX_PATH ..
  make
````
