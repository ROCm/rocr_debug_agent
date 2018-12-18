# ROCr Debug Agent

The ROCr Debug Agent is a library that can be loaded by ROCm Platform Runtime to provide the following functionality:

* Print the state of wavefronts that report memory violation or upon executing a "s_trap 2" instruction.
* Allows SIGINT (`ctrl c`) or SIGTERM (`kill -15`) to print wavefront state of aborted GPU dispatches.
* It is enabled on Vega10 (since ROCm1.9), Vega20 (since ROCm2.0) GPUs.

## Usage

To use the ROCr Debug Agent set the following environment variable:

```sh
export HSA_TOOLS_LIB=librocr_debug_agent64.so
```

This will use the ROCr Debug Agent library installed at /opt/rocm/lib/librocr_debug_agent64.so by default since the ROCm installation adds /opt/rocm/lib to the system library path. To use a different version set the LD_LIBRARY_PATH, for example:

```sh
export LD_LIBRARY_PATH=/path_to_directory_containing_librocr_debug_agent64.so
```

To display the machine code instructions of wavefronts, together with the source text location, the ROCr Debug Agent using the llvm-objdump tool. (1) Compile the code object with "-g" for OpenCL, or "-gline-tables-only" for HIP/hcc. (2) Ensure a llvm-objdump that supports AMD GCN GPUs is on your '$PATH`. For example, for the ROCm1.9:

```sh
export PATH=/opt/rocm/opencl/bin/x86_64/:$PATH
```

Execute your application.

If the application encounters a GPU error, it will display the wavefront state of the GPU to `stdout`. Possible error states include:

* The GPU executes a memory instruction that causes a memory violation. This is reported as an XNACK error state.
* Queue error.
* The GPU executes an `S_TRAP` instruction. The `__builtin_trap()` language builtin can be used to generate a `S_TRAP`.
* A SIGINT (`ctrl c`) or SIGTERM (`kill -15`) signal is sent to the application while executing GPU code. Enabled by the `ROCM_DEBUG_ENABLE_LINUX_SIGNALS` environment variable.

For example, a sample print out for GPU memory fault is:

```
Memory access fault by GPU agent: AMD gfx900
Node: 1
Address: 0x18DB4xxx (page not present;write access to a read-only page;)

64 wavefront(s) found in XNACK error state @PC: 0x0000001100E01310
printing the first one:

   EXEC: 0xFFFFFFFFFFFFFFFF
 STATUS: 0x00412460
TRAPSTS: 0x30000000
     M0: 0x00001010

     s0: 0x00C00000    s1: 0x80000010    s2: 0x10000000    s3: 0x00EA4FAC
     s4: 0x17D78400    s5: 0x00000000    s6: 0x01039000    s7: 0x00000000
     s8: 0x00000000    s9: 0x00000000   s10: 0x17D78400   s11: 0x04000000
    s12: 0x00000000   s13: 0x00000000   s14: 0x00000000   s15: 0x00000000
    s16: 0x0103C000   s17: 0x00000000   s18: 0x00000000   s19: 0x00000000
    s20: 0x01037060   s21: 0x00000000   s22: 0x00000000   s23: 0x00000011
    s24: 0x00004000   s25: 0x00010000   s26: 0x04C00000   s27: 0x00000010
    s28: 0xFFFFFFFF   s29: 0xFFFFFFFF   s30: 0x00000000   s31: 0x00000000

Lane 0x0
     v0: 0x00000003    v1: 0x18DB4400    v2: 0x18DB4400    v3: 0x00000000
     v4: 0x00000000    v5: 0x00000000    v6: 0x00700000    v7: 0x00800000
Lane 0x1
     v0: 0x00000004    v1: 0x18DB4400    v2: 0x18DB4400    v3: 0x00000000
     v4: 0x00000000    v5: 0x00000000    v6: 0x00700000    v7: 0x00800000
Lane 0x2
     v0: 0x00000005    v1: 0x18DB4400    v2: 0x18DB4400    v3: 0x00000000
     v4: 0x00000000    v5: 0x00000000    v6: 0x00700000    v7: 0x00800000
Lane 0x3
     v0: 0x00000006    v1: 0x18DB4400    v2: 0x18DB4400    v3: 0x00000000
     v4: 0x00000000    v5: 0x00000000    v6: 0x00700000    v7: 0x00800000

    .
    .
    .

Lane 0x3C
     v0: 0x0000001F    v1: 0x18DB4400    v2: 0x18DB4400    v3: 0x00000000
     v4: 0x00000000    v5: 0x00000000    v6: 0x00700000    v7: 0x00800000
Lane 0x3D
     v0: 0x00000020    v1: 0x18DB4400    v2: 0x18DB4400    v3: 0x00000000
     v4: 0x00000000    v5: 0x00000000    v6: 0x00700000    v7: 0x00800000
Lane 0x3E
     v0: 0x00000021    v1: 0x18DB4400    v2: 0x18DB4400    v3: 0x00000000
     v4: 0x00000000    v5: 0x00000000    v6: 0x00700000    v7: 0x00800000
Lane 0x3F
     v0: 0x00000022    v1: 0x18DB4400    v2: 0x18DB4400    v3: 0x00000000
     v4: 0x00000000    v5: 0x00000000    v6: 0x00700000    v7: 0x00800000

Faulty Code Object:

/tmp/ROCm_Tmp_PID_5764/ROCm_Code_Object_0:      file format ELF64-amdgpu-hsacobj

Disassembly of section .text:
the_kernel:
; /home/qingchuan/tests/faulty_test/vector_add_kernel.cl:12
; d[100000000] = ga[gid & 31];
        v_mov_b32_e32 v1, v2                                       // 0000000012F0: 7E020302
        v_mov_b32_e32 v4, v3                                       // 0000000012F4: 7E080303
        v_add_i32_e32 v1, vcc, s10, v1                             // 0000000012F8: 3202020A
        v_mov_b32_e32 v5, s22                                      // 0000000012FC: 7E0A0216
        v_addc_u32_e32 v4, vcc, v4, v5, vcc                        // 000000001300: 38080B04
        v_mov_b32_e32 v2, v1                                       // 000000001304: 7E040301
        v_mov_b32_e32 v3, v4                                       // 000000001308: 7E060304
        s_waitcnt lgkmcnt(0)                                       // 00000000130C: BF8CC07F
        flat_store_dword v[2:3], v0                                // 000000001310: DC700000 00000002
; /home/qingchuan/tests/faulty_test/vector_add_kernel.cl:13
; }
        s_endpgm                                                   // 000000001318: BF810000

Faulty PC offset: 1310

Aborted (core dumped)
```

## Options

### Dump Output

By default the wavefront dump is sent to `stdout`.

To save to a file use:

```sh
export ROCM_DEBUG_WAVE_STATE_DUMP=file
```

This will create a file called `ROCm_Wave_State_Dump` in code object directory (see below).

To return to the default `stdout` use either of the following:

```sh
export ROCM_DEBUG_WAVE_STATE_DUMP=stdout
unset ROCM_DEBUG_WAVE_STATE_DUMP
```

### Linux Signal Control

The following environment variable can be used to enable dumping wavefront states when SIGINT (`ctrl c`) or SIGTERM (`kill -15`) is sent to the application:

```sh
export ROCM_DEBUG_ENABLE_LINUX_SIGNALS=1
```

Either of the following will disable this behavior:

```sh
export ROCM_DEBUG_ENABLE_LINUX_SIGNALS=0
unset ROCM_DEBUG_ENABLE_LINUX_SIGNALS
```

### Code Object Saving

When the ROCr Debug Agent is enabled, each GPU code object loaded by the ROCm Platform Runtime will be saved in a file in the code object directory. By default the code object directory is `/tmp/ROCm_Tmp_PID_XXXX/` where `XXXX` is the application process ID. The code object directory can be specified using the following environent variable:

```sh
export ROCM_DEBUG_SAVE_CODE_OBJECT=code_object_directory
```

This will use the path `/code_object_directory`.

Loaded code objects will be saved in files named `ROCm_Code_Object_N` where N is a unique integer starting at 0 of the order in which the code object was loaded.

If the default code object directory is used, then the saved code object file will be deleted when it is unloaded with the ROCm Platform Runtime, and the complete code object directory will be deleted when the application exits normally. If a code object directory path is specified then neither the saved code objects, nor the code object directory will be deleted.

To return to using the default code object directory use:

```sh
unset ROCM_DEBUG_SAVE_CODE_OBJECT
```

### Logging

By default ROCr Debug Agent logging is disabled. It can be enabled to display to `stdout` using:

```sh
export ROCM_DEBUG_ENABLE_AGENTLOG=stdout
```

Or to a file using:

```sh
export ROCM_DEBUG_ENABLE_AGENTLOG=<filename>
```

Which will write to the file `<filename>_AgentLog_PID_XXXX.log`.

To disable logging use:

```sh
unset ROCM_DEBUG_ENABLE_AGENTLOG
```

## Repository Contents

* `src`
  * Contains the sources for building the ROCr Debug Agent. See the `README.md` for directions.
* `test`
  * Contains the tests for the ROCr Debug Agent. See the `README.md` for directions.
