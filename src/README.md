# Build the debug agent
To build the debug agent compatible version of the runtime and the libhsakmt library must be available. The latest version of these files can be obtained from ROCR-Runtime and ROCT-Thunk-Interface repository, available here:
* ROCR-Runtime https://github.com/RadeonOpenCompute/ROCR-Runtime
* ROCT-Thunk-Interface: https://github.com/RadeonOpenCompute/ROCT-Thunk-Interface


Build the AMD HSA Debug Agent Library
  1. Defined ROCR-Runtime header and libraries path, ROCT-Thunk-Interface header and libraries path, and lightening compiler path in Debug_Agent_PREFIX_PATH if they are not in the default "/opt/rocm/" location.
  2. Make an new folder called build under root folder
  3. Enter into folder of build, and run CMAKE to generate makefile and make it

  For example, from the src repository execute:
```
  export Debug_Agent_PREFIX_PATH=Path/to/ROC/Runtime/headers;Path/to/ROC/Runtime/libraries;
                                 Path/to/ROC/Thunk/headers;Path/to/ROC/Thunk/libraries;
                                 Path/to/Lightening/Compiler/Clang
  mkdir build
  cd build
  cmake -DCMAKE_PREFIX_PATH=$Debug_Agent_PREFIX_PATH ..
  make
```
