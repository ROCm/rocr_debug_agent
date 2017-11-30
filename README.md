# rocr_debug_agent
The rocr_debug_agent repository provides the components required to build library for dump GPU memory fault info.
The rocr_debug_agent components are used by ROCm-GDB to support debugging GPU kernels on ROCm.

# Package Contents
The ROCm GPU Debug SDK includes the source code is briefly listed below
* Source code
  * HSA Debug Agent: The HSA Debug Agent is a library injected into an HSA application by the ROCR-Runtime. The source code for the Agent is provided in *src*.

# Debug Agent User Guide
* Enable debug agent (export librocr_debug_agent64.so path if it is not install in /opt/rocm/lib)
  * `export HSA_TOOLS_LIB=librocr_debug_agent64.so`
  * `export LD_LIBRARY_PATH=/path/to/librocr_debug_agent64.so>`

* Dump wave states.
  * Include llvm-objdump in `$PATH`. (llvm-objdump needs to support target “amdgcn - AMD GCN GPUs”).
  * Run the application
  * Wave states will be printed to stdout by default in case of memory fault, queue error, and trap instructions (__builtin_trap(); or __builtin_debugtrap();).
  * Use env variable `ROCM_DEBUG_WAVE_STATE_DUMP` to save dump to file.
```
    export ROCM_DEBUG_WAVE_STATE_DUMP=stdout    // Print dump to stdout, default
    export ROCM_DEBUG_WAVE_STATE_DUMP=file      // Save dump to file ROCm_Wave_State_Dump in code object folder
```
  * Use env variable `ROCM_DEBUG_ENABLE_LINUX_SIGNALS` to enable dump wave states when SIGINT (`ctrl c`) or SIGTERM (`kill -15`)
```
    export ROCM_DEBUG_ENABLE_LINUX_SIGNALS=0    // Disable dump for Linux signal
    export ROCM_DEBUG_ENABLE_LINUX_SIGNALS=1    // Enable dump for Linux signal
```

* Save GPU code object files.
  * By default code objects will be saved in /tmp/ROCm_Tmp_PID_XXXX/ when loaded, and deleted when unload.
  * Use env variable `ROCM_DEBUG_SAVE_CODE_OBJECT` to specify the path to save all GPU code object files.
```
    export ROCM_DEBUG_SAVE_CODE_OBJECT=/code/object/file/path
```

* Debug Agent Logging.
  Debug Agent has internal Logging. It is disabled by default, and can be enabled by env variables.
  * `export ROCM_DEBUG_ENABLE_AGENTLOG=stdout` will write debug agent log to cout.
  * `export ROCM_DEBUG_ENABLE_AGENTLOG=<filename>` will write debug agent log to file.

* A sample print out for GPU memory fault
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
