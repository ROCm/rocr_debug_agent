# Debug Agent Test User Guide
Test following ROC Debug Agent functions.
  * test 0: Runtime load/unload debug agent when running normal vector add program.
  * test 1: Print the state of wavefronts that report memory violation.
  * test 2: Print the state of wavefronts upon executing a "s_trap 2" instruction.

Build the test
```
  mkdir build
  cd build
  cmake ..
  make
```

Run the test
* export LD_LIBRARY_PATH with path librocr_debug_agent64.so, if it is not at the default location /opt/rocm/lib
```
export LD_LIBRARY_PATH=/path/to/librocr_debug_agent64.so
```
* Include llvm-objdump in `$PATH`, if it is not at the default location /opt/rocm/opencl/bin/x86_64/
  llvm-objdump needs to support target “amdgcn - AMD GCN GPUs”.
```
export PATH=path/to/llvm-objdump:$PATH
```
* export HSA_TOOLS_LIB with librocr_debug_agent64.so to let ROC runtime load debug agent library.
```
export HSA_TOOLS_LIB=librocr_debug_agent64.so
```
* Run all tests
```
./rocr_debug_agent_test
```
* Run single test, for example test 0
```
./rocr_debug_agent_test 0
```

Expected output

test 0
```
  *** Debug Agent Test: Debug agent tests start ***
  *** Debug Agent Test: VectorAddNormal start ***
  *** Debug Agent Test: VectorAddNormal end ***
  *** Debug Agent Test: Debug agent test finished ***
```

test1
```
  *** Debug Agent Test: VectorAddDebugTrapTest start ***
[rocr debug agent]:
Queue error state in GPU agent: AMD gfx900
Node: 2
Queue ID: 140068470472704 (Debug trap;)


64 wavefront(s) found in @PC: 0x00007F63364042EC
printing the first one:

   EXEC: 0xFFFFFFFFFFFFFFFF
 STATUS: 0x00412461
TRAPSTS: 0x20000000
     M0: 0x80000000

     s0: 0x3B6D8000    s1: 0x00007F64    s2: 0x0E000000    s3: 0x00EA4FAC
     s4: 0x00000000    s5: 0x00000000    s6: 0x3B6D8000    s7: 0x00007F64
     s8: 0x3B6C6000    s9: 0x00007F64   s10: 0x3B6C8000   s11: 0x00007F64
    s12: 0x00000002   s13: 0x03800000   s14: 0x3B678000   s15: 0x00007F64
    s16: 0x3B670000   s17: 0x00007F64   s18: 0x00000040   s19: 0x0000FFFF
    s20: 0x3B6C4070   s21: 0x00007F64   s22: 0x36405070   s23: 0x00007F63
    s24: 0x00003800   s25: 0x0000E000   s26: 0x2D600000   s27: 0x00007F64
    s28: 0x01034000   s29: 0x00A00000   s30: 0x00000000   s31: 0x00000000

Lane 0x0
     v0: 0x0000000C    v1: 0x3B670000    v2: 0x3B670000    v3: 0x00007F64
     v4: 0x00000000    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E80E38D   v10: 0xBF5F2DF3   v11: 0x3EDB6DB8
Lane 0x1
     v0: 0x0000000C    v1: 0x3B670004    v2: 0x3B670004    v3: 0x00007F64
     v4: 0x00000004    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E82AAA9   v10: 0xBF4D20D2   v11: 0x3EDB6DB8
Lane 0x2
     v0: 0x0000000A    v1: 0x3B670008    v2: 0x3B670008    v3: 0x00007F64
     v4: 0x00000008    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E80E38D   v10: 0xBF5F2DF3   v11: 0x3F24924A
Lane 0x3
     v0: 0x00000007    v1: 0x3B67000C    v2: 0x3B67000C    v3: 0x00007F64
     v4: 0x0000000C    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E82AAA9   v10: 0xBF4D20D2   v11: 0x3F24924A
Lane 0x4
     v0: 0x0000000B    v1: 0x3B670010    v2: 0x3B670010    v3: 0x00007F64
     v4: 0x00000010    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8471C6   v10: 0xBF506906   v11: 0x3EF6DB70
Lane 0x5
     v0: 0x00000004    v1: 0x3B670014    v2: 0x3B670014    v3: 0x00007F64
     v4: 0x00000014    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8638E2   v10: 0xBF400000   v11: 0x3EF6DB70
Lane 0x6
     v0: 0x00000011    v1: 0x3B670018    v2: 0x3B670018    v3: 0x00007F64
     v4: 0x00000018    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8471C6   v10: 0xBF506906   v11: 0x3F24924A
Lane 0x7
     v0: 0x0000000F    v1: 0x3B67001C    v2: 0x3B67001C    v3: 0x00007F64
     v4: 0x0000001C    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8638E2   v10: 0xBF400000   v11: 0x3F24924A
Lane 0x8
     v0: 0x00000010    v1: 0x3B670020    v2: 0x3B670020    v3: 0x00007F64
     v4: 0x00000020    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E80E38D   v10: 0xBF41A41A   v11: 0x3EF6DB70
Lane 0x9
     v0: 0x00000009    v1: 0x3B670024    v2: 0x3B670024    v3: 0x00007F64
     v4: 0x00000024    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E82AAA9   v10: 0xBF32DF2E   v11: 0x3EF6DB70
Lane 0xA
     v0: 0x0000000B    v1: 0x3B670028    v2: 0x3B670028    v3: 0x00007F64
     v4: 0x00000028    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E80E38D   v10: 0xBF41A41A   v11: 0x3F24924A
Lane 0xB
     v0: 0x00000011    v1: 0x3B67002C    v2: 0x3B67002C    v3: 0x00007F64
     v4: 0x0000002C    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E82AAA9   v10: 0xBF32DF2E   v11: 0x3F24924A
Lane 0xC
     v0: 0x0000000D    v1: 0x3B670030    v2: 0x3B670030    v3: 0x00007F64
     v4: 0x00000030    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8471C6   v10: 0xBF348348   v11: 0x3EF6DB70
Lane 0xD
     v0: 0x00000009    v1: 0x3B670034    v2: 0x3B670034    v3: 0x00007F64
     v4: 0x00000034    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8638E2   v10: 0xBF25BE5C   v11: 0x3EF6DB70
Lane 0xE
     v0: 0x0000000F    v1: 0x3B670038    v2: 0x3B670038    v3: 0x00007F64
     v4: 0x00000038    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8471C6   v10: 0xBF348348   v11: 0x3F24924A
Lane 0xF
     v0: 0x00000006    v1: 0x3B67003C    v2: 0x3B67003C    v3: 0x00007F64
     v4: 0x0000003C    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8638E2   v10: 0xBF25BE5C   v11: 0x3F24924A
Lane 0x10
     v0: 0x0000000B    v1: 0x3B670040    v2: 0x3B670040    v3: 0x00007F64
     v4: 0x00000040    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E87FFFE   v10: 0xBF276276   v11: 0x3EF6DB70
Lane 0x11
     v0: 0x0000000C    v1: 0x3B670044    v2: 0x3B670044    v3: 0x00007F64
     v4: 0x00000044    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E89C71A   v10: 0xBF0EC4EC   v11: 0x3EF6DB70
Lane 0x12
     v0: 0x0000000B    v1: 0x3B670048    v2: 0x3B670048    v3: 0x00007F64
     v4: 0x00000048    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E87FFFE   v10: 0xBF276276   v11: 0x3F24924A
Lane 0x13
     v0: 0x00000006    v1: 0x3B67004C    v2: 0x3B67004C    v3: 0x00007F64
     v4: 0x0000004C    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E89C71A   v10: 0xBF0EC4EC   v11: 0x3F24924A
Lane 0x14
     v0: 0x0000000C    v1: 0x3B670050    v2: 0x3B670050    v3: 0x00007F64
     v4: 0x00000050    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8B8E37   v10: 0xBF120D20   v11: 0x3EF6DB70
Lane 0x15
     v0: 0x0000000F    v1: 0x3B670054    v2: 0x3B670054    v3: 0x00007F64
     v4: 0x00000054    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8D5553   v10: 0xBEFCB7CC   v11: 0x3EF6DB70
Lane 0x16
     v0: 0x00000007    v1: 0x3B670058    v2: 0x3B670058    v3: 0x00007F64
     v4: 0x00000058    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8B8E37   v10: 0xBF120D20   v11: 0x3F24924A
Lane 0x17
     v0: 0x00000012    v1: 0x3B67005C    v2: 0x3B67005C    v3: 0x00007F64
     v4: 0x0000005C    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8D5553   v10: 0xBEFCB7CC   v11: 0x3F24924A
Lane 0x18
     v0: 0x00000006    v1: 0x3B670060    v2: 0x3B670060    v3: 0x00007F64
     v4: 0x00000060    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E87FFFE   v10: 0xBF000000   v11: 0x3EF6DB70
Lane 0x19
     v0: 0x0000000F    v1: 0x3B670064    v2: 0x3B670064    v3: 0x00007F64
     v4: 0x00000064    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E89C71A   v10: 0xBEE5BE5C   v11: 0x3EF6DB70
Lane 0x1A
     v0: 0x0000000C    v1: 0x3B670068    v2: 0x3B670068    v3: 0x00007F64
     v4: 0x00000068    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E87FFFE   v10: 0xBF000000   v11: 0x3F24924A
Lane 0x1B
     v0: 0x0000000C    v1: 0x3B67006C    v2: 0x3B67006C    v3: 0x00007F64
     v4: 0x0000006C    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E89C71A   v10: 0xBEE5BE5C   v11: 0x3F24924A
Lane 0x1C
     v0: 0x0000000F    v1: 0x3B670070    v2: 0x3B670070    v3: 0x00007F64
     v4: 0x00000070    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8B8E37   v10: 0xBEEC4EC4   v11: 0x3EC92494
Lane 0x1D
     v0: 0x0000000C    v1: 0x3B670074    v2: 0x3B670074    v3: 0x00007F64
     v4: 0x00000074    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8D5553   v10: 0xBEC83484   v11: 0x3EC92494
Lane 0x1E
     v0: 0x0000000E    v1: 0x3B670078    v2: 0x3B670078    v3: 0x00007F64
     v4: 0x00000078    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8B8E37   v10: 0xBEEC4EC4   v11: 0x3F24924A
Lane 0x1F
     v0: 0x0000000C    v1: 0x3B67007C    v2: 0x3B67007C    v3: 0x00007F64
     v4: 0x0000007C    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8D5553   v10: 0xBEC83484   v11: 0x3F24924A
Lane 0x20
     v0: 0x00000006    v1: 0x3B670080    v2: 0x3B670080    v3: 0x00007F64
     v4: 0x00000080    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E80E38D   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x21
     v0: 0x00000008    v1: 0x3B670084    v2: 0x3B670084    v3: 0x00007F64
     v4: 0x00000084    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E82AAA9   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x22
     v0: 0x00000005    v1: 0x3B670088    v2: 0x3B670088    v3: 0x00007F64
     v4: 0x00000088    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E80E38D   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x23
     v0: 0x00000005    v1: 0x3B67008C    v2: 0x3B67008C    v3: 0x00007F64
     v4: 0x0000008C    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E82AAA9   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x24
     v0: 0x00000008    v1: 0x3B670090    v2: 0x3B670090    v3: 0x00007F64
     v4: 0x00000090    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8471C6   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x25
     v0: 0x00000008    v1: 0x3B670094    v2: 0x3B670094    v3: 0x00007F64
     v4: 0x00000094    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8638E2   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x26
     v0: 0x0000000C    v1: 0x3B670098    v2: 0x3B670098    v3: 0x00007F64
     v4: 0x00000098    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8471C6   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x27
     v0: 0x0000000A    v1: 0x3B67009C    v2: 0x3B67009C    v3: 0x00007F64
     v4: 0x0000009C    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8638E2   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x28
     v0: 0x0000000F    v1: 0x3B6700A0    v2: 0x3B6700A0    v3: 0x00007F64
     v4: 0x000000A0    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E80E38D   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x29
     v0: 0x0000000F    v1: 0x3B6700A4    v2: 0x3B6700A4    v3: 0x00007F64
     v4: 0x000000A4    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E82AAA9   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x2A
     v0: 0x00000009    v1: 0x3B6700A8    v2: 0x3B6700A8    v3: 0x00007F64
     v4: 0x000000A8    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E80E38D   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x2B
     v0: 0x0000000A    v1: 0x3B6700AC    v2: 0x3B6700AC    v3: 0x00007F64
     v4: 0x000000AC    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E82AAA9   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x2C
     v0: 0x00000007    v1: 0x3B6700B0    v2: 0x3B6700B0    v3: 0x00007F64
     v4: 0x000000B0    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8471C6   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x2D
     v0: 0x00000011    v1: 0x3B6700B4    v2: 0x3B6700B4    v3: 0x00007F64
     v4: 0x000000B4    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8638E2   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x2E
     v0: 0x0000000C    v1: 0x3B6700B8    v2: 0x3B6700B8    v3: 0x00007F64
     v4: 0x000000B8    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8471C6   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x2F
     v0: 0x00000008    v1: 0x3B6700BC    v2: 0x3B6700BC    v3: 0x00007F64
     v4: 0x000000BC    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8638E2   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x30
     v0: 0x00000003    v1: 0x3B6700C0    v2: 0x3B6700C0    v3: 0x00007F64
     v4: 0x000000C0    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E87FFFE   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x31
     v0: 0x00000008    v1: 0x3B6700C4    v2: 0x3B6700C4    v3: 0x00007F64
     v4: 0x000000C4    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E89C71A   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x32
     v0: 0x0000000C    v1: 0x3B6700C8    v2: 0x3B6700C8    v3: 0x00007F64
     v4: 0x000000C8    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E87FFFE   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x33
     v0: 0x00000009    v1: 0x3B6700CC    v2: 0x3B6700CC    v3: 0x00007F64
     v4: 0x000000CC    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E89C71A   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x34
     v0: 0x0000000C    v1: 0x3B6700D0    v2: 0x3B6700D0    v3: 0x00007F64
     v4: 0x000000D0    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8B8E37   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x35
     v0: 0x00000008    v1: 0x3B6700D4    v2: 0x3B6700D4    v3: 0x00007F64
     v4: 0x000000D4    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8D5553   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x36
     v0: 0x0000000F    v1: 0x3B6700D8    v2: 0x3B6700D8    v3: 0x00007F64
     v4: 0x000000D8    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8B8E37   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x37
     v0: 0x0000000A    v1: 0x3B6700DC    v2: 0x3B6700DC    v3: 0x00007F64
     v4: 0x000000DC    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8D5553   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x38
     v0: 0x00000003    v1: 0x3B6700E0    v2: 0x3B6700E0    v3: 0x00007F64
     v4: 0x000000E0    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E87FFFE   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x39
     v0: 0x00000009    v1: 0x3B6700E4    v2: 0x3B6700E4    v3: 0x00007F64
     v4: 0x000000E4    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E89C71A   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x3A
     v0: 0x0000000C    v1: 0x3B6700E8    v2: 0x3B6700E8    v3: 0x00007F64
     v4: 0x000000E8    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E87FFFE   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x3B
     v0: 0x00000007    v1: 0x3B6700EC    v2: 0x3B6700EC    v3: 0x00007F64
     v4: 0x000000EC    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E89C71A   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x3C
     v0: 0x0000000C    v1: 0x3B6700F0    v2: 0x3B6700F0    v3: 0x00007F64
     v4: 0x000000F0    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8B8E37   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x3D
     v0: 0x0000000B    v1: 0x3B6700F4    v2: 0x3B6700F4    v3: 0x00007F64
     v4: 0x000000F4    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8D5553   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x3E
     v0: 0x00000005    v1: 0x3B6700F8    v2: 0x3B6700F8    v3: 0x00007F64
     v4: 0x000000F8    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8B8E37   v10: 0x3EC6C6C7   v11: 0x3F800000
Lane 0x3F
     v0: 0x0000000F    v1: 0x3B6700FC    v2: 0x3B6700FC    v3: 0x00007F64
     v4: 0x000000FC    v5: 0x00000000    v6: 0x00007F64    v7: 0x00007F64
     v8: 0x00000000    v9: 0x3E8D5553   v10: 0x3EC6C6C7   v11: 0x3F800000

Code Object:/tmp/ROCm_Tmp_PID_4391/ROCm_Code_Object_0:      file format ELF64-amdgpu

Disassembly of section .text:
vector_add_debug_trap:
; /home/qingchuan/workspace/compute-master/hsa/rocr_debug_agent/test/vector_add_debug_trap_kernel.cl:7
; c[gid] = a[gid] + b[gid];
        v_cndmask_b32_e32 v1, v1, v6, vcc                          // 0000000012CC: 00020D01
        v_addc_co_u32_e64 v6, s[4:5], v7, v8, s[4:5]               // 0000000012D0: D11C0406 00121107
        v_mov_b32_e32 v2, v1                                       // 0000000012D8: 7E040301
        v_mov_b32_e32 v3, v6                                       // 0000000012DC: 7E060306
        global_store_dword v[2:3], v0, off                         // 0000000012E0: DC708000 007F0002
; /home/qingchuan/workspace/compute-master/hsa/rocr_debug_agent/test/vector_add_debug_trap_kernel.cl:8
; __builtin_trap();
        s_mov_b64 s[0:1], s[6:7]                                   // 0000000012E8: BE800106
        s_trap 2                                                   // 0000000012EC: BF920002
; /home/qingchuan/workspace/compute-master/hsa/rocr_debug_agent/test/vector_add_debug_trap_kernel.cl:9
; }
        s_endpgm                                                   // 0000000012F0: BF810000

PC offset: 12EC

runtime catched trap instruction successfully
custom queue error handler completed successfully
  *** Debug Agent Test: VectorAddDebugTrapTest end ***
  *** Debug Agent Test: Debug agent test finished ***
  **************************
```

test 2 (Note: since ROC runtime abort after kernel memeory access fault, program abort at the end is expected (seeing printout `Aborted (core dumped)`)
```
  *** Debug Agent Test: Debug agent tests start ***
  *** Debug Agent Test: VectorAddMemoryFaultTest start ***
[rocr debug agent]:
Memory access fault by GPU agent: AMD gfx900
Node: 2
Address: 0x7FEDDB176xxx (page not present;write access to a read-only page;)


13 wavefront(s) found in @PC: 0x00007FECD6004304
printing the first one:

   EXEC: 0xFFFFFFFFFFFFFFFF
 STATUS: 0x00412461
TRAPSTS: 0x30000100
     M0: 0x80000000

     s0: 0xC9A00000    s1: 0x80007FED    s2: 0x0E000000    s3: 0x00EA4FAC
     s4: 0x00000000    s5: 0x00000000    s6: 0xDB1C6000    s7: 0x00007FED
     s8: 0xDB1C8000    s9: 0x00007FED   s10: 0x00000002   s11: 0x07020000
    s12: 0xDB178000   s13: 0x00007FED   s14: 0xDB170000   s15: 0x00007FED
    s16: 0x0000000A   s17: 0x0000FFFF   s18: 0xD6005070   s19: 0x00007FEC
    s20: 0xDB1C4070   s21: 0x00007FED   s22: 0xD6005070   s23: 0x00007FEC
    s24: 0x00003800   s25: 0x0000E000   s26: 0xD0A20000   s27: 0x00007FED
    s28: 0xFC000000   s29: 0xFFFFFFFF   s30: 0x00000000   s31: 0x00000000

Lane 0x0
     v0: 0xDB173C00    v1: 0x0000000E    v2: 0xDB173C00    v3: 0x00007FED
     v4: 0x00003C00    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF800000   v11: 0xBF0E38E4
Lane 0x1
     v0: 0xDB173C28    v1: 0x0000000E    v2: 0xDB173C28    v3: 0x00007FED
     v4: 0x00003C28    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF46318C   v11: 0xBF0E38E4
Lane 0x2
     v0: 0xDB173C50    v1: 0x0000000D    v2: 0xDB173C50    v3: 0x00007FED
     v4: 0x00003C50    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF800000   v11: 0x3F0E38E4
Lane 0x3
     v0: 0xDB173C78    v1: 0x0000000A    v2: 0xDB173C78    v3: 0x00007FED
     v4: 0x00003C78    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF46318C   v11: 0x3F0E38E4
Lane 0x4
     v0: 0xDB173CA0    v1: 0x00000018    v2: 0xDB173CA0    v3: 0x00007FED
     v4: 0x00003CA0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF46318C   v11: 0xBF0E38E4
Lane 0x5
     v0: 0xDB173CC8    v1: 0x0000000B    v2: 0xDB173CC8    v3: 0x00007FED
     v4: 0x00003CC8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBEE739D0   v11: 0xBF0E38E4
Lane 0x6
     v0: 0xDB173CF0    v1: 0x00000008    v2: 0xDB173CF0    v3: 0x00007FED
     v4: 0x00003CF0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF46318C   v11: 0x3F0E38E4
Lane 0x7
     v0: 0xDB173D18    v1: 0x00000007    v2: 0xDB173D18    v3: 0x00007FED
     v4: 0x00003D18    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBEE739D0   v11: 0x3F0E38E4
Lane 0x8
     v0: 0xDB173D40    v1: 0x0000000C    v2: 0xDB173D40    v3: 0x00007FED
     v4: 0x00003D40    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF042108   v11: 0xBEAAAAAA
Lane 0x9
     v0: 0xDB173D68    v1: 0x0000000E    v2: 0xDB173D68    v3: 0x00007FED
     v4: 0x00003D68    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBEB5AD6C   v11: 0xBEAAAAAA
Lane 0xA
     v0: 0xDB173D90    v1: 0x00000008    v2: 0xDB173D90    v3: 0x00007FED
     v4: 0x00003D90    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF042108   v11: 0x3F0E38E4
Lane 0xB
     v0: 0xDB173DB8    v1: 0x0000000A    v2: 0xDB173DB8    v3: 0x00007FED
     v4: 0x00003DB8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBEB5AD6C   v11: 0x3F0E38E4
Lane 0xC
     v0: 0xDB173DE0    v1: 0x00000012    v2: 0xDB173DE0    v3: 0x00007FED
     v4: 0x00003DE0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBEC6318C   v11: 0xBF0E38E4
Lane 0xD
     v0: 0xDB173E08    v1: 0x00000009    v2: 0xDB173E08    v3: 0x00007FED
     v4: 0x00003E08    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBD842110   v11: 0xBF0E38E4
Lane 0xE
     v0: 0xDB173E30    v1: 0x0000000E    v2: 0xDB173E30    v3: 0x00007FED
     v4: 0x00003E30    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBEC6318C   v11: 0x3F0E38E4
Lane 0xF
     v0: 0xDB173E58    v1: 0x00000004    v2: 0xDB173E58    v3: 0x00007FED
     v4: 0x00003E58    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBD842110   v11: 0x3F0E38E4
Lane 0x10
     v0: 0xDB173E80    v1: 0x0000000E    v2: 0xDB173E80    v3: 0x00007FED
     v4: 0x00003E80    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBE042108   v11: 0xBF0E38E4
Lane 0x11
     v0: 0xDB173EA8    v1: 0x0000000E    v2: 0xDB173EA8    v3: 0x00007FED
     v4: 0x00003EA8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0x3E463188   v11: 0xBF0E38E4
Lane 0x12
     v0: 0xDB173ED0    v1: 0x0000000C    v2: 0xDB173ED0    v3: 0x00007FED
     v4: 0x00003ED0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBE042108   v11: 0x3F0E38E4
Lane 0x13
     v0: 0xDB173EF8    v1: 0x0000000E    v2: 0xDB173EF8    v3: 0x00007FED
     v4: 0x00003EF8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0x3E463188   v11: 0x3F0E38E4
Lane 0x14
     v0: 0xDB173F20    v1: 0x00000009    v2: 0xDB173F20    v3: 0x00007FED
     v4: 0x00003F20    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0x3E6739D0   v11: 0xBF0E38E4
Lane 0x15
     v0: 0xDB173F48    v1: 0x00000010    v2: 0xDB173F48    v3: 0x00007FED
     v4: 0x00003F48    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0x3F1CE738   v11: 0xBF0E38E4
Lane 0x16
     v0: 0xDB173F70    v1: 0x0000000B    v2: 0xDB173F70    v3: 0x00007FED
     v4: 0x00003F70    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0x3E6739D0   v11: 0x3F0E38E4
Lane 0x17
     v0: 0xDB173F98    v1: 0x00000005    v2: 0xDB173F98    v3: 0x00007FED
     v4: 0x00003F98    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0x3F1CE738   v11: 0x3F0E38E4
Lane 0x18
     v0: 0xDB173FC0    v1: 0x00000015    v2: 0xDB173FC0    v3: 0x00007FED
     v4: 0x00003FC0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0x3F14A528   v11: 0xBF0E38E4
Lane 0x19
     v0: 0xDB173FE8    v1: 0x0000000B    v2: 0xDB173FE8    v3: 0x00007FED
     v4: 0x00003FE8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0x3F842108   v11: 0xBF0E38E4
Lane 0x1A
     v0: 0xDB174010    v1: 0x00000011    v2: 0xDB174010    v3: 0x00007FED
     v4: 0x00004010    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0x3F14A528   v11: 0x3F0E38E4
Lane 0x1B
     v0: 0xDB174038    v1: 0x0000000B    v2: 0xDB174038    v3: 0x00007FED
     v4: 0x00004038    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0x3F842108   v11: 0x3F0E38E4
Lane 0x1C
     v0: 0xDB174060    v1: 0x00000008    v2: 0xDB174060    v3: 0x00007FED
     v4: 0x00004060    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF700000   v11: 0xBF7C28F6
Lane 0x1D
     v0: 0xDB174088    v1: 0x0000000A    v2: 0xDB174088    v3: 0x00007FED
     v4: 0x00004088    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF6D999A   v11: 0xBF7C28F6
Lane 0x1E
     v0: 0xDB1740B0    v1: 0x00000003    v2: 0xDB1740B0    v3: 0x00007FED
     v4: 0x000040B0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF700000   v11: 0xBF78BF26
Lane 0x1F
     v0: 0xDB1740D8    v1: 0x00000008    v2: 0xDB1740D8    v3: 0x00007FED
     v4: 0x000040D8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF6D999A   v11: 0xBF78BF26
Lane 0x20
     v0: 0xDB174100    v1: 0x0000000B    v2: 0xDB174100    v3: 0x00007FED
     v4: 0x00004100    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF6D999A   v11: 0xBF7C28F6
Lane 0x21
     v0: 0xDB174128    v1: 0x00000014    v2: 0xDB174128    v3: 0x00007FED
     v4: 0x00004128    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF6B3333   v11: 0xBF7C28F6
Lane 0x22
     v0: 0xDB174150    v1: 0x0000000A    v2: 0xDB174150    v3: 0x00007FED
     v4: 0x00004150    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF6D999A   v11: 0xBF78BF26
Lane 0x23
     v0: 0xDB174178    v1: 0x0000000E    v2: 0xDB174178    v3: 0x00007FED
     v4: 0x00004178    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF6B3333   v11: 0xBF78BF26
Lane 0x24
     v0: 0xDB1741A0    v1: 0x0000000B    v2: 0xDB1741A0    v3: 0x00007FED
     v4: 0x000041A0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF6B7777   v11: 0xBF7B4E82
Lane 0x25
     v0: 0xDB1741C8    v1: 0x0000000C    v2: 0xDB1741C8    v3: 0x00007FED
     v4: 0x000041C8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF69999A   v11: 0xBF7B4E82
Lane 0x26
     v0: 0xDB1741F0    v1: 0x0000000B    v2: 0xDB1741F0    v3: 0x00007FED
     v4: 0x000041F0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF6B7777   v11: 0xBF7A06D4
Lane 0x27
     v0: 0xDB174218    v1: 0x00000012    v2: 0xDB174218    v3: 0x00007FED
     v4: 0x00004218    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF69999A   v11: 0xBF7A06D4
Lane 0x28
     v0: 0xDB174240    v1: 0x00000005    v2: 0xDB174240    v3: 0x00007FED
     v4: 0x00004240    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF6A2222   v11: 0xBF7D036A
Lane 0x29
     v0: 0xDB174268    v1: 0x00000009    v2: 0xDB174268    v3: 0x00007FED
     v4: 0x00004268    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF666666   v11: 0xBF7D036A
Lane 0x2A
     v0: 0xDB174290    v1: 0x00000002    v2: 0xDB174290    v3: 0x00007FED
     v4: 0x00004290    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF6A2222   v11: 0xBF78BF26
Lane 0x2B
     v0: 0xDB1742B8    v1: 0x00000012    v2: 0xDB1742B8    v3: 0x00007FED
     v4: 0x000042B8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF666666   v11: 0xBF78BF26
Lane 0x2C
     v0: 0xDB1742E0    v1: 0x00000010    v2: 0xDB1742E0    v3: 0x00007FED
     v4: 0x000042E0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF66EEEF   v11: 0xBF7D036A
Lane 0x2D
     v0: 0xDB174308    v1: 0x0000000B    v2: 0xDB174308    v3: 0x00007FED
     v4: 0x00004308    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF644444   v11: 0xBF7D036A
Lane 0x2E
     v0: 0xDB174330    v1: 0x00000012    v2: 0xDB174330    v3: 0x00007FED
     v4: 0x00004330    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF66EEEF   v11: 0xBF78BF26
Lane 0x2F
     v0: 0xDB174358    v1: 0x00000011    v2: 0xDB174358    v3: 0x00007FED
     v4: 0x00004358    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF644444   v11: 0xBF78BF26
Lane 0x30
     v0: 0xDB174380    v1: 0x00000004    v2: 0xDB174380    v3: 0x00007FED
     v4: 0x00004380    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF64CCCD   v11: 0xBF7B4E82
Lane 0x31
     v0: 0xDB1743A8    v1: 0x0000000E    v2: 0xDB1743A8    v3: 0x00007FED
     v4: 0x000043A8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF62EEEF   v11: 0xBF7B4E82
Lane 0x32
     v0: 0xDB1743D0    v1: 0x0000000E    v2: 0xDB1743D0    v3: 0x00007FED
     v4: 0x000043D0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF64CCCD   v11: 0xBF7A06D4
Lane 0x33
     v0: 0xDB1743F8    v1: 0x00000013    v2: 0xDB1743F8    v3: 0x00007FED
     v4: 0x000043F8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF62EEEF   v11: 0xBF7A06D4
Lane 0x34
     v0: 0xDB174420    v1: 0x00000009    v2: 0xDB174420    v3: 0x00007FED
     v4: 0x00004420    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF63BBBC   v11: 0xBF7D036A
Lane 0x35
     v0: 0xDB174448    v1: 0x00000009    v2: 0xDB174448    v3: 0x00007FED
     v4: 0x00004448    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF611111   v11: 0xBF7D036A
Lane 0x36
     v0: 0xDB174470    v1: 0x00000011    v2: 0xDB174470    v3: 0x00007FED
     v4: 0x00004470    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF63BBBC   v11: 0xBF78BF26
Lane 0x37
     v0: 0xDB174498    v1: 0x0000000C    v2: 0xDB174498    v3: 0x00007FED
     v4: 0x00004498    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF611111   v11: 0xBF78BF26
Lane 0x38
     v0: 0xDB1744C0    v1: 0x00000011    v2: 0xDB1744C0    v3: 0x00007FED
     v4: 0x000044C0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF615555   v11: 0xBF7D036A
Lane 0x39
     v0: 0xDB1744E8    v1: 0x00000006    v2: 0xDB1744E8    v3: 0x00007FED
     v4: 0x000044E8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF5EAAAA   v11: 0xBF7D036A
Lane 0x3A
     v0: 0xDB174510    v1: 0x0000000A    v2: 0xDB174510    v3: 0x00007FED
     v4: 0x00004510    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF615555   v11: 0xBF78BF26
Lane 0x3B
     v0: 0xDB174538    v1: 0x00000014    v2: 0xDB174538    v3: 0x00007FED
     v4: 0x00004538    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF5EAAAA   v11: 0xBF78BF26
Lane 0x3C
     v0: 0xDB174560    v1: 0x00000013    v2: 0xDB174560    v3: 0x00007FED
     v4: 0x00004560    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF5EEEEF   v11: 0xBF7D036A
Lane 0x3D
     v0: 0xDB174588    v1: 0x0000000B    v2: 0xDB174588    v3: 0x00007FED
     v4: 0x00004588    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF5C4444   v11: 0xBF7D036A
Lane 0x3E
     v0: 0xDB1745B0    v1: 0x0000000B    v2: 0xDB1745B0    v3: 0x00007FED
     v4: 0x000045B0    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF5EEEEF   v11: 0xBF78BF26
Lane 0x3F
     v0: 0xDB1745D8    v1: 0x0000000B    v2: 0xDB1745D8    v3: 0x00007FED
     v4: 0x000045D8    v5: 0x00000000    v6: 0x00007FED    v7: 0x00007FED
     v8: 0x00000000    v9: 0x00000000   v10: 0xBF5C4444   v11: 0xBF78BF26

Code Object:/tmp/ROCm_Tmp_PID_4406/ROCm_Code_Object_0:      file format ELF64-amdgpu

Disassembly of section .text:
vector_add_memory_fault:
; /home/qingchuan/workspace/compute-master/hsa/rocr_debug_agent/test/vector_add_memory_fault_kernel.cl:7
; c[gid*10] = a[gid] + b[gid];
        v_mov_b32_e32 v7, v3                                       // 0000000012E4: 7E0E0303
        v_mov_b32_e32 v8, v5                                       // 0000000012E8: 7E100305
        v_add_co_u32_e64 v0, s[4:5], v0, v6                        // 0000000012EC: D1190400 00020D00
        v_addc_co_u32_e64 v6, s[4:5], v7, v8, s[4:5]               // 0000000012F4: D11C0406 00121107
        v_mov_b32_e32 v2, v0                                       // 0000000012FC: 7E040300
        v_mov_b32_e32 v3, v6                                       // 000000001300: 7E060306
        global_store_dword v[2:3], v1, off                         // 000000001304: DC708000 007F0102
; /home/qingchuan/workspace/compute-master/hsa/rocr_debug_agent/test/vector_add_memory_fault_kernel.cl:8
; }
        s_endpgm                                                   // 00000000130C: BF810000

PC offset: 1304

Aborted (core dumped)
```
