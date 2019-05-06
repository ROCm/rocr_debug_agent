////////////////////////////////////////////////////////////////////////////////
//
// The University of Illinois/NCSA
// Open Source License (NCSA)
//
// Copyright (c) 2018, Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal with the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
//  - Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimers.
//  - Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimers in
//    the documentation and/or other materials provided with the distribution.
//  - Neither the names of Advanced Micro Devices, Inc,
//    nor the names of its contributors may be used to endorse or promote
//    products derived from this Software without specific prior written
//    permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS WITH THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////////

.text

.hsa_code_object_version 2,1

.hsa_code_object_isa 9,0,0,"AMD","AMDGPU"

.amdgpu_hsa_kernel debug_trap_handler

debug_trap_handler:
.amd_kernel_code_t
  kernel_code_entry_byte_offset = 256
  enable_sgpr_queue_ptr = 1
  granulated_workitem_vgpr_count = 1
  granulated_wavefront_sgpr_count = 1
  user_sgpr_count = 2
.end_amd_kernel_code_t

.set SQ_WAVE_PC_HI_ADDRESS_MASK            , 0xFFFF
.set SQ_WAVE_PC_HI_TRAP_ID_SHIFT           , 16
.set SQ_WAVE_PC_HI_TRAP_ID_SIZE            , 8
.set SQ_WAVE_PC_HI_TRAP_ID_BFE             , (SQ_WAVE_PC_HI_TRAP_ID_SHIFT | (SQ_WAVE_PC_HI_TRAP_ID_SIZE << 16))
.set SQ_WAVE_TRAPSTS_EXCP_MASK             , 0x1FF
.set SQ_WAVE_TRAPSTS_ILLEGAL_INST_MASK     , 0x800
.set SQ_WAVE_TRAPSTS_EXCP_HI_MASK          , 0x7000
.set SQ_WAVE_TRAPSTS_ALL_EXCPS_MASK        , (SQ_WAVE_TRAPSTS_EXCP_MASK | SQ_WAVE_TRAPSTS_ILLEGAL_INST_MASK | SQ_WAVE_TRAPSTS_EXCP_HI_MASK)
.set SQ_WAVE_STATUS_HALT_MASK              , 0x2000
.set SQ_WAVE_IB_STS_RCNT_MASK              , 0x1F0000
.set SQ_WAVE_IB_STS_FIRST_REPLAY_MASK      , 0x8000
.set SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT     , 15
.set SQ_WAVE_IB_STS_FIRST_REPLAY_RCNT_MASK , (SQ_WAVE_IB_STS_FIRST_REPLAY_MASK | SQ_WAVE_IB_STS_RCNT_MASK)
.set IB_STS_SAVE_FIRST_REPLAY_SHIFT        , 26
.set IB_STS_SAVE_FIRST_REPLAY_REL_SHIFT    , (IB_STS_SAVE_FIRST_REPLAY_SHIFT - SQ_WAVE_IB_STS_FIRST_REPLAY_SHIFT)
.set TTMP11_SINGLE_STEP_DISABLED_MASK      , 0x8000

// ABI between first and second level trap handler:
//   ttmp0 = PC[31:0]
//   ttmp1 = 0[2:0], PCRewind[3:0], HostTrap[0], TrapId[7:0], PC[47:32]
//   ttmp11 = SQ_WAVE_IB_STS[20:15], 0[9:0], SingleStep[1], 0[7:0], NoScratch[0], WaveIdInWG[5:0]
//   ttmp12 = SQ_WAVE_STATUS
//   ttmp14 = TMA[31:0]
//   ttmp15 = TMA[63:32]

trap_entry:
  // If not a trap then skip queue signalling.
  s_bfe_u32            ttmp2, ttmp1, SQ_WAVE_PC_HI_TRAP_ID_BFE
  s_cbranch_scc0       L_NOT_TRAP

  // If not debugtrap (s_trap 1) or llvm.trap (s_trap 2) then signal debugger.
  s_cmp_ge_u32         ttmp2, 0x3
  s_cbranch_scc1       L_SEND_DEBUG_SIGNAL

  // Retrieve amd_queue_t.queue_inactive_signal from s[0:1].
  s_load_dwordx2       [ttmp2, ttmp3], s[0:1], 0xC0 glc
  s_waitcnt            lgkmcnt(0)

  // Signal queue with trap error value.
  s_mov_b32            ttmp14, 0x80000000
  s_mov_b32            ttmp15, 0x0
  s_branch             L_SEND_SIGNAL

L_NOT_TRAP:
  // If any exception raised then signal debugger.
  s_getreg_b32         ttmp2, hwreg(HW_REG_TRAPSTS)
  s_and_b32            ttmp2, ttmp2, SQ_WAVE_TRAPSTS_ALL_EXCPS_MASK
  s_cbranch_scc1       L_SEND_DEBUG_SIGNAL

L_SINGLE_STEP:
  // If single-step notification enabled then signal debugger.
  s_and_b32            ttmp2, ttmp11, TTMP11_SINGLE_STEP_DISABLED_MASK
  s_cbranch_scc1       L_SEND_DEBUG_SIGNAL

  // Mask non-PC bits for conditional breakpoint tests.
  s_and_b32            ttmp1, ttmp1, SQ_WAVE_PC_HI_ADDRESS_MASK

L_COND_BREAK_LOOP:
  // Advance to next conditional breakpoint in TMA page.
  s_add_u32            ttmp14, ttmp14, 0x8

  // Fetch conditional breakpoint PC.
  s_load_dwordx2       [ttmp2, ttmp3], [ttmp14, ttmp15], 0x0 glc
  s_waitcnt            lgkmcnt(0)

  // Terminate breakpoint search on zero value and exit trap.
  s_and_b64            [ttmp2, ttmp3], [ttmp2, ttmp3], [ttmp2, ttmp3]
  s_cbranch_scc0       L_EXIT_TRAP

  // If breakpoint does not match current PC then loop.
  s_cmp_eq_u64         [ttmp0, ttmp1], [ttmp2, ttmp3]
  s_cbranch_scc0       L_COND_BREAK_LOOP

  // Reset TMA to beginning of page.
  s_andn2_b32          ttmp14, ttmp14, 0xFFF;

L_SEND_DEBUG_SIGNAL:
  // Fetch debug trap signal from TMA.
  s_load_dwordx2       [ttmp2, ttmp3], [ttmp14, ttmp15], 0x0 glc
  s_waitcnt            lgkmcnt(0)
  s_branch             L_SET_EVENT

L_SEND_SIGNAL:
  // Set signal value and retrieve old value.
  s_atomic_swap_x2     [ttmp14, ttmp15], [ttmp2, ttmp3], 0x8 glc
  s_waitcnt            lgkmcnt(0)

  // Skip event trigger if the signal value was already non-zero.
  s_or_b32             ttmp14, ttmp14, ttmp15
  s_cbranch_scc1       L_SIGNAL_DONE

L_SET_EVENT:
  // Check for a non-NULL signal event mailbox.
  s_load_dwordx2       [ttmp14, ttmp15], [ttmp2, ttmp3], 0x10 glc
  s_waitcnt            lgkmcnt(0)
  s_and_b64            [ttmp14, ttmp15], [ttmp14, ttmp15], [ttmp14, ttmp15]
  s_cbranch_scc0       L_SIGNAL_DONE

  // Load the signal event value.
  s_load_dword         ttmp2, [ttmp2, ttmp3], 0x18 glc
  s_waitcnt            lgkmcnt(0)

  // Write the signal event value to the mailbox.
  s_store_dword        ttmp2, [ttmp14, ttmp15], 0x0 glc
  s_waitcnt            lgkmcnt(0)

  // Send an interrupt to trigger event notification.
  s_sendmsg            sendmsg(MSG_INTERRUPT)

L_SIGNAL_DONE:
  // Halt the wavefront.
  s_or_b32             ttmp12, ttmp12, SQ_WAVE_STATUS_HALT_MASK

L_EXIT_TRAP:
  // Restore SQ_WAVE_IB_STS.
  s_lshr_b32           ttmp2, ttmp11, IB_STS_SAVE_FIRST_REPLAY_REL_SHIFT
  s_and_b32            ttmp2, ttmp2, SQ_WAVE_IB_STS_FIRST_REPLAY_RCNT_MASK
  s_setreg_b32         hwreg(HW_REG_IB_STS), ttmp2

  // Restore SQ_WAVE_STATUS.
  s_and_b64            exec, exec, exec // Restore STATUS.EXECZ, not writable by s_setreg_b32
  s_and_b64            vcc, vcc, vcc    // Restore STATUS.VCCZ, not writable by s_setreg_b32
  s_setreg_b32         hwreg(HW_REG_STATUS), ttmp12

  // Return to shader at unmodified PC.
  s_rfe_b64            [ttmp0, ttmp1]
