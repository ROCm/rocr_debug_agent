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

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

// HSA headers
#include <hsakmt.h>
#include <hsa_api_trace.h>

// Debug Agent Headers
#include "AgentLogging.h"
#include "AgentUtils.h"
#include "HSADebugAgentGDBInterface.h"
#include "HSADebugAgent.h"
#include "HSADebugInfo.h"

// Total number of loaded code object during execution (only increase)
static uint32_t gs_numCodeObject = 0;

DebugAgentQueueWaveMap allQueueWaves;
DebugAgentQueueInfoMap allDebugAgentQueueInfo;

// queue and wave
std::map<uint64_t, std::pair<uint64_t, WaveStateInfo*>> FindFaultyWaves(GPUAgentInfo *pAgent);


DebugAgentStatus ProcessQueueWaveStates(GPUAgentInfo* pAgent, QueueInfo *pQueue)
{
    struct context_save_area_header_t
    {
        uint32_t ctrl_stack_offset;
        uint32_t ctrl_stack_size;
        uint32_t wave_state_offset;
        uint32_t wave_state_size;
    } *header;

    if (!pQueue->pSaveAreaHeader)
    {
        AGENT_ERROR("Cannot get context save area header.");
        return DEBUG_AGENT_STATUS_FAILURE;
    }

    HSA_QUEUEID queueId = pQueue->queueId;
    header = reinterpret_cast<struct context_save_area_header_t *>(pQueue->pSaveAreaHeader);

    if ((header->ctrl_stack_offset + header->ctrl_stack_size) != (header->wave_state_offset - header->wave_state_size))
    {
        AGENT_ERROR("Ctrl stack / context save address check fail.");
        return DEBUG_AGENT_STATUS_FAILURE;
    }

    // The control stack is processed from start to end.
    // The save area is processed from end to start.
    uint32_t *ctl_stack = reinterpret_cast<uint32_t *>((uint64_t)pQueue->pSaveAreaHeader + header->ctrl_stack_offset);
    uint32_t *wave_area = reinterpret_cast<uint32_t *>((uint64_t)pQueue->pSaveAreaHeader + header->wave_state_offset);
    uint32_t ctl_stack_ndw = uint32_t(header->ctrl_stack_size / sizeof(uint32_t));

    // Control stack persists resource allocation until changed by a command.
    uint32_t vgprs_size_dw = 0;
    uint32_t accvgprs_size_dw = 0;
    uint32_t sgprs_size_dw = 0;
    uint32_t lds_size_dw = 0;
    //  TODO: check wave front size and hw regsiter size for arch
    const uint32_t wave_front_size = 0x40;

    // LDS is saved per-workgroup but the stack is parsed per-wavefront.
    // Track the LDS save area for the current workgroup.
    uint32_t *lds = nullptr;

    // Only decode waves when debug agent is used stand alone.
    if (!g_gdbAttached)
    {
        allQueueWaves.insert(std::pair<uint64_t, WaveStates>(queueId, WaveStates()));

        // Parse each write to COMPUTE_RELAUNCH in sequence.
        // First two dwords are SET_SH_REG leader.
        // TODO:
        for (uint32_t idx = 2; idx < ctl_stack_ndw; ++idx)
        {
            uint32_t relaunch = ctl_stack[idx];
            bool is_event = COMPUTE_RELAUNCH_IS_EVENT(relaunch);
            bool is_state = COMPUTE_RELAUNCH_IS_STATE(relaunch);

            // TODO: check control records are present, and their values first.
            if (is_state && !is_event)
            {
                vgprs_size_dw = (0x1 + COMPUTE_RELAUNCH_PAYLOAD_VGPRS(relaunch)) * 0x4;
                // TODO: This is a temp fix, each target should be able to have its own layout.
                if (pAgent->hasAccVgprs)
                {
                    accvgprs_size_dw = vgprs_size_dw;
                }
                // SGPRs do not include trap temp.
                sgprs_size_dw = ((0x1 + COMPUTE_RELAUNCH_PAYLOAD_SGPRS(relaunch)) - 0x1) * 0x10;
                lds_size_dw = COMPUTE_RELAUNCH_PAYLOAD_LDS_SIZE(relaunch) * 0x80;

            }
            else if (!is_state && !is_event)
            {
                // Reference to one wavefront in the save area.
                bool first_wave_in_group = COMPUTE_RELAUNCH_PAYLOAD_FIRST_WAVE(relaunch);

                // Save area layout is fixed by context save trap handler and SPI.
                // offset of dw
                uint32_t vgprs_offset = 0x0;
                uint32_t accvgprs_offset = vgprs_offset + vgprs_size_dw * wave_front_size;
                uint32_t sgprs_offset = accvgprs_offset + accvgprs_size_dw * wave_front_size;
                uint32_t hwregs_offset = sgprs_offset + sgprs_size_dw;
                uint32_t lds_offset = hwregs_offset + 0x20;
                uint32_t unused_offset = lds_offset + (first_wave_in_group ? lds_size_dw : 0x0);
                uint32_t wave_area_size = unused_offset + 0x10;
                uint32_t hwreg_m0_offset = hwregs_offset + 0x0;
                uint32_t hwreg_pc_lo_offset = hwregs_offset + 0x1;
                uint32_t hwreg_pc_hi_offset = hwregs_offset + 0x2;
                uint32_t hwreg_exec_lo_offset = hwregs_offset + 0x3;
                uint32_t hwreg_exec_hi_offset = hwregs_offset + 0x4;
                uint32_t hwreg_status_offset = hwregs_offset + 0x5;
                uint32_t hwreg_trapsts_offset = hwregs_offset + 0x6;
                // uint32_t hwreg_xnack_mask_lo_offset = hwregs_offset + 0x7;
                // uint32_t hwreg_xnack_mask_hi_offset = hwregs_offset + 0x8;
                // uint32_t hwreg_mode_offset = hwregs_offset + 0x9;
                // uint32_t hwreg_ttmp6_offset = hwregs_offset + 0x10;
                // uint32_t hwreg_ttmp7_offset = hwregs_offset + 0x11;
                // uint32_t hwreg_ttmp8_offset = hwregs_offset + 0x12;
                // uint32_t hwreg_ttmp9_offset = hwregs_offset + 0x13;
                // uint32_t hwreg_ttmp10_offset = hwregs_offset + 0x14;
                // uint32_t hwreg_ttmp11_offset = hwregs_offset + 0x15;
                // uint32_t hwreg_ttmp13_offset = hwregs_offset + 0x16;
                // uint32_t hwreg_ttmp14_offset = hwregs_offset + 0x17;
                // uint32_t hwreg_ttmp15_offset = hwregs_offset + 0x18;

                // Find beginning of wavefront state in the save area.
                wave_area -= wave_area_size;

                if (first_wave_in_group)
                {
                    // Track the LDS save area for this workgroup.
                    if (lds_size_dw > 0)
                    {
                        lds = wave_area + lds_offset;
                    }
                    else
                    {
                        lds = nullptr;
                    }
                }

                // Save wave state in debug agent.
                WaveStateInfo waveList;

                // TODO: check the value from context save area
                waveList.numSgprs = sgprs_size_dw;
                waveList.sgprs = wave_area + sgprs_offset;
                waveList.numVgprs = vgprs_size_dw;
                waveList.numAccVgprs = accvgprs_size_dw;
                waveList.numVgprLanes = wave_front_size;
                waveList.numAccVgprLanes = wave_front_size;
                waveList.vgprs = wave_area + vgprs_offset;
                waveList.accvgprs = wave_area + accvgprs_offset;
                waveList.regs.pc = (uint64_t(wave_area[hwreg_pc_lo_offset]) |
                                    (uint64_t(wave_area[hwreg_pc_hi_offset]) << 0x20));
                waveList.regs.exec = (uint64_t(wave_area[hwreg_exec_lo_offset]) |
                                        (uint64_t(wave_area[hwreg_exec_hi_offset]) << 0x20));
                waveList.regs.status = wave_area[hwreg_status_offset];
                waveList.regs.trapsts = wave_area[hwreg_trapsts_offset];
                waveList.regs.m0 = wave_area[hwreg_m0_offset];
                waveList.ldsSizeDw = lds_size_dw;
                waveList.lds = lds;

                allQueueWaves[queueId].push_back(waveList);

                if (SQ_WAVE_TRAPSTS_XNACK_ERROR(waveList.regs.trapsts))
                {
                    pQueue->queueStatus = QUEUE_STATUS_FAILURE;
                }
            }
        }

        if (((uint64_t)pQueue->pSaveAreaHeader + header->wave_state_offset - header->wave_state_size) != (uint64_t)wave_area)
        {
            AGENT_ERROR("Context save size check fail.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }
    }

    return DEBUG_AGENT_STATUS_SUCCESS;
}

DebugAgentStatus PreemptAgentQueues(GPUAgentInfo* pAgent)
{
    HSAKMT_STATUS kmt_status = HSAKMT_STATUS_SUCCESS;
    std::vector<HSA_QUEUEID> queueIds;
    std::vector<QueueInfo *> queueInfos;

    for (QueueInfo *pQueue = pAgent->pQueueList; pQueue; pQueue = pQueue->pNext)
    {
        queueIds.emplace_back (pQueue->queueId);
        queueInfos.emplace_back (pQueue);
    }

    // preempt the queues
    kmt_status = hsaKmtQueueSuspend(INVALID_PID,
                                    queueIds.size(),
                                    queueIds.data(),
                                    0,
                                    0);
    if (kmt_status != HSAKMT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Cannot preempt queues.");
        return DEBUG_AGENT_STATUS_FAILURE;
    }

    // get the queue wave states
    for (auto &&pQueueInfo : queueInfos)
    {
        DebugAgentStatus status = DEBUG_AGENT_STATUS_SUCCESS;
        status = ProcessQueueWaveStates(pAgent, pQueueInfo);
        if (status != DEBUG_AGENT_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot get queue preemption.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }
    }

    return DEBUG_AGENT_STATUS_SUCCESS;
}

DebugAgentStatus PreemptAllQueues()
{
    GPUAgentInfo *pAgent = _r_rocm_debug_info.pAgentList;
    while (pAgent != nullptr)
    {
        DebugAgentStatus status = DEBUG_AGENT_STATUS_SUCCESS;
        status = PreemptAgentQueues(pAgent);
        if (status != DEBUG_AGENT_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot get queue preemption.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }
        pAgent = pAgent->pNext;
    }
    return DEBUG_AGENT_STATUS_SUCCESS;
}

DebugAgentStatus ResumeAgentQueues(GPUAgentInfo* pAgent)
{
    HSAKMT_STATUS kmt_status = HSAKMT_STATUS_SUCCESS;
    std::vector<HSA_QUEUEID> queue_ids;

    for (QueueInfo *pQueue = pAgent->pQueueList; pQueue; pQueue = pQueue->pNext)
        if (pQueue->queueStatus == HSA_STATUS_SUCCESS)
            queue_ids.emplace_back (pQueue->queueId);

    // resume the queues
    kmt_status = hsaKmtQueueResume(INVALID_PID,
                                   queue_ids.size(),
                                   queue_ids.data(),
                                   0);
    if (kmt_status != HSAKMT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Cannot resume queues.");
        return DEBUG_AGENT_STATUS_FAILURE;
    }

    return DEBUG_AGENT_STATUS_SUCCESS;
}

DebugAgentStatus ResumeAllQueues()
{
    GPUAgentInfo *pAgent = _r_rocm_debug_info.pAgentList;
    while (pAgent != nullptr)
    {
        DebugAgentStatus status = DEBUG_AGENT_STATUS_SUCCESS;
        status = ResumeAgentQueues(pAgent);
        if (status != DEBUG_AGENT_STATUS_SUCCESS)
        {
            AGENT_ERROR("Cannot resume queues.");
            return DEBUG_AGENT_STATUS_FAILURE;
        }
        pAgent = pAgent->pNext;
    }
    return DEBUG_AGENT_STATUS_SUCCESS;
}

void PrintWaves(GPUAgentInfo* pAgent, std::map<uint64_t, std::pair<uint64_t, WaveStateInfo *>> waves)
{
    std::stringstream err;

    for (auto it = waves.begin(); it != waves.end(); it++)
    {
        uint64_t numFautlyWaveByPC = it->second.first;
        WaveStateInfo *pWaveState = it->second.second;

        err << std::dec << numFautlyWaveByPC << " wavefront(s) found in @PC: 0x"
            << std::setw(0x10) << std::setfill('0') << std::hex << std::uppercase << pWaveState->regs.pc << "\n";
        err << "printing the first one: "
            << "\n\n";
        err << "   EXEC: 0x" << std::setw(0x10) << std::setfill('0') << std::hex << std::uppercase << pWaveState->regs.exec << "\n";
        err << " STATUS: 0x" << std::setw(0x8) << std::setfill('0') << std::hex << std::uppercase << pWaveState->regs.status << "\n";
        err << "TRAPSTS: 0x" << std::setw(0x8) << std::setfill('0') << std::hex << std::uppercase << pWaveState->regs.trapsts << "\n";
        err << "     M0: 0x" << std::setw(0x8) << std::setfill('0') << std::hex << std::uppercase << pWaveState->regs.m0 << "\n\n";

        uint32_t n_sgpr_cols = 4;
        uint32_t n_sgpr_rows = pWaveState->numSgprs / n_sgpr_cols;

        for (uint32_t sgpr_row = 0; sgpr_row < n_sgpr_rows; ++sgpr_row)
        {
            err << " ";
            for (uint32_t sgpr_col = 0; sgpr_col < n_sgpr_cols; ++sgpr_col)
            {
                uint32_t sgpr_idx = (sgpr_row * n_sgpr_cols) + sgpr_col;
                uint32_t sgpr_val = pWaveState->sgprs[sgpr_idx];

                std::stringstream sgpr_str;
                sgpr_str << "s" << sgpr_idx;

                err << std::setw(6) << std::setfill(' ') << sgpr_str.str();
                err << ": 0x" << std::setw(8) << std::setfill('0') << std::hex << std::uppercase << sgpr_val;
            }
            err << "\n";
        }

        err << "\n";

        uint32_t n_vgpr_cols = 4;
        uint32_t n_vgpr_rows = pWaveState->numVgprs / n_vgpr_cols;

        for (uint32_t lane_idx = 0; lane_idx < pWaveState->numVgprLanes; ++lane_idx)
        {
            err << "Lane 0x" << std::hex << std::uppercase << lane_idx << "\n";
            for (uint32_t vgpr_row = 0; vgpr_row < n_vgpr_rows; ++vgpr_row)
            {
                err << " ";
                for (uint32_t vgpr_col = 0; vgpr_col < n_vgpr_cols; ++vgpr_col)
                {
                    uint32_t vgpr_idx = (vgpr_row * n_vgpr_cols) + vgpr_col;
                    uint32_t vgpr_val = pWaveState->vgprs[(vgpr_idx * pWaveState->numVgprLanes) + lane_idx];

                    std::stringstream vgpr_str;
                    vgpr_str << "v" << vgpr_idx;

                    err << std::setw(6) << std::setfill(' ') << vgpr_str.str();
                    err << ": 0x" << std::setw(8) << std::setfill('0') << std::hex << std::uppercase << vgpr_val;
                }
                err << "\n";
            }
        }
        err << "\n";

        if (pWaveState->numAccVgprs)
        {
            uint32_t n_accvgpr_cols = 4;
            uint32_t n_accvgpr_rows = pWaveState->numAccVgprs / n_accvgpr_cols;

            for (uint32_t lane_idx = 0; lane_idx < pWaveState->numAccVgprLanes; ++lane_idx)
            {
                err << "ACC Lane 0x" << std::hex << std::uppercase << lane_idx << "\n";
                for (uint32_t accvgpr_row = 0; accvgpr_row < n_accvgpr_rows; ++accvgpr_row)
                {
                    err << " ";
                    for (uint32_t accvgpr_col = 0; accvgpr_col < n_accvgpr_cols; ++accvgpr_col)
                    {
                        uint32_t accvgpr_idx = (accvgpr_row * n_accvgpr_cols) + accvgpr_col;
                        uint32_t accvgpr_val = pWaveState->accvgprs[(accvgpr_idx * pWaveState->numAccVgprLanes) + lane_idx];

                        std::stringstream accvgpr_str;
                        accvgpr_str << "acc" << accvgpr_idx;

                        err << std::setw(6) << std::setfill(' ') << accvgpr_str.str();
                        err << ": 0x" << std::setw(8) << std::setfill('0') << std::hex << std::uppercase << accvgpr_val;
                    }
                    err << "\n";
                }
            }
            err << "\n";
        }

        if (pWaveState->lds)
        {
            err << "LDS:\n\n";

            uint32_t n_lds_cols = 4;
            uint32_t n_lds_rows = pWaveState->ldsSizeDw / n_lds_cols;

            for (uint32_t lds_row = 0; lds_row < n_lds_rows; ++lds_row)
            {
                uint32_t lds_addr = lds_row * n_lds_cols * 4;
                err << "0x" << std::setw(4) << std::setfill('0') << std::hex << std::uppercase << lds_addr << ":";
                for (uint32_t lds_col = 0; lds_col < n_lds_cols; ++lds_col)
                {
                    uint32_t lds_idx = (lds_row * n_lds_cols) + lds_col;
                    uint32_t lds_val = pWaveState->lds[lds_idx];
                    err << "  0x" << std::setw(8) << std::setfill('0') << std::hex << std::uppercase << lds_val;
                }
                err << "\n";
            }
            err << "\n";
        }

        char *code_obj_path = nullptr;
        uint64_t pc_code_obj_offset = 0;
        ExecutableInfo *pExec = _r_rocm_debug_info.pExecutableList;
        while(pExec)
        {
            CodeObjectInfo *pList = pExec->pCodeObjectList;
            while (pList)
            {
                if ((pWaveState->regs.pc >= pList->addrLoaded) &&
                    (pWaveState->regs.pc < (pList->addrLoaded + pList->sizeLoaded)))
                {
                    code_obj_path = &(pList->path[0]);
                    pc_code_obj_offset = pWaveState->regs.pc - pList->addrLoaded;
                    break;
                }
                pList = pList->pNext;
            }
            pExec = pExec->pNext;
        }

        if (code_obj_path != nullptr)
        {
            // Invoke binutils objdump on the code object.
            int pipe_fd[2];
            if (::pipe(pipe_fd) != 0)
            {
                AGENT_ERROR("Cannot create pipe for llvm-objdump.");
                return;
            }

            pid_t pid = ::fork();

            if (pid == 0)
            {
                ::dup2(pipe_fd[1], STDOUT_FILENO);
                ::dup2(pipe_fd[1], STDERR_FILENO);
                ::close(pipe_fd[0]);
                ::close(pipe_fd[1]);

                // Disassemble X bytes before/after the PC.
                uint32_t disasm_context = 0x20;

                std::stringstream  mcpuString;
                uint64_t targetName = atoi(&(pAgent->agentName[7]));
                mcpuString << "-mcpu=gfx" << targetName;
                std::stringstream arg_start_addr, arg_stop_addr;
                arg_start_addr << "--start-address=0x" << std::hex << std::uppercase << (pc_code_obj_offset - disasm_context);
                arg_stop_addr << "--stop-address=0x" << std::hex << std::uppercase << (pc_code_obj_offset + disasm_context);

                std::exit(execlp("/opt/rocm/opencl/bin/x86_64/llvm-objdump", "llvm-objdump", "-triple=amdgcn-amd-amdhsa",
                                 (char *)(mcpuString.str().c_str()), "-disassemble", "-source", "-line-numbers", (char *)(arg_start_addr.str().c_str()),
                                 (char *)(arg_stop_addr.str().c_str()), (char *)code_obj_path, (char *)NULL));
            }

            // Collect the output of objdump.
            ::close(pipe_fd[1]);

            std::vector<char> objdump_out_buf;
            std::vector<char> buf(0x1000);
            ssize_t n_read_b;

            while ((n_read_b = read(pipe_fd[0], buf.data(), buf.size())) > 0)
            {
                objdump_out_buf.insert(objdump_out_buf.end(), &buf[0], &buf[n_read_b]);
            }

            ::close(pipe_fd[0]);

            int child_status = 0;
            int ret = ::waitpid(pid, &child_status, 0);

            if (ret != -1 && child_status == 0)
            {
                std::string objdump_out(objdump_out_buf.begin(), objdump_out_buf.end());
                err << "Code Object:\n";
                err << objdump_out;
                err << "\nPC offset: " << std::hex << std::uppercase << pc_code_obj_offset << "\n\n";
            }
            else
            {
                err << "Code Object:\n"
                    << code_obj_path << "\n\n";
                err << "(Disassembly unavailable - is amdgcn-capable llvm-objdump in /opt/rocm/opencl/bin/x86_64/)\n\n";
            }
        }
        else
        {
            err << "(Cannot match PC to a loaded code object)\n\n";
        }
    }

    char *pDumpNameEnvVar;
    pDumpNameEnvVar = std::getenv("ROCM_DEBUG_WAVE_STATE_DUMP");

    if (pDumpNameEnvVar == nullptr)
    {
        std::cout << err.str();
    }
    else
    {
        std::string envName(pDumpNameEnvVar);
        if (envName == "stdout")
        {
            std::cout << err.str();
        }
        else if (envName == "file")
        {
            SaveWaveStateDumpToFile(err);
        }
        else
        {
            AGENT_WARNING("Invalid value for ROCM_DEBUG_WAVE_STATE_DUMP, printing dump to stdout.");
            std::cout << err.str();
        }
    }
}

GPUAgentInfo *GetAgentFromList(uint32_t nodeId)
{
    GPUAgentInfo *pList = _r_rocm_debug_info.pAgentList;
    while (pList != nullptr)
    {
        if (pList->nodeId == nodeId)
        {
            return pList;
        }
        pList = pList->pNext;
    }
    return nullptr;
}

GPUAgentInfo *GetAgentFromList(void* agentHandle)
{
    GPUAgentInfo *pList = _r_rocm_debug_info.pAgentList;
    while (pList != nullptr)
    {
        if (pList->agent == agentHandle)
        {
            return pList;
        }
        pList = pList->pNext;
    }
    return nullptr;
}

GPUAgentInfo *GetAgentByQueueID(uint64_t queueId)
{
    GPUAgentInfo *pAgentList = _r_rocm_debug_info.pAgentList;
    QueueInfo *pQueueList = nullptr;
    while (pAgentList != nullptr)
    {
        pQueueList = pAgentList->pQueueList;
        while (pQueueList != nullptr)
        {
            if (pQueueList->queueId == queueId)
            {
                return pAgentList;
            }
            pQueueList = pQueueList->pNext;
        }
        pAgentList = pAgentList->pNext;
    }
    return nullptr;
}

DebugAgentStatus addQueueToList(uint32_t nodeId, QueueInfo *pQueue)
{
    GPUAgentInfo *pAgent = GetAgentFromList(nodeId);
    DebugAgentStatus statusAddToList = AddToLinkListEnd<QueueInfo>(pQueue, &(pAgent->pQueueList));
    return statusAddToList;
}

QueueInfo *GetQueueFromList(uint32_t nodeId, uint64_t queueId)
{
    QueueInfo *pList = (GetAgentFromList(nodeId))->pQueueList;
    while (pList != nullptr)
    {
        if (pList->queueId == queueId)
        {
            return pList;
        }
        pList = pList->pNext;
    }
    return nullptr;
}

QueueInfo *GetQueueFromList(uint64_t queueId)
{
    GPUAgentInfo *pAgentList = _r_rocm_debug_info.pAgentList;
    QueueInfo *pQueueList = nullptr;
    while (pAgentList != nullptr)
    {
        pQueueList = pAgentList->pQueueList;
        while (pQueueList != nullptr)
        {
            if (pQueueList->queueId == queueId)
            {
                return pQueueList;
            }
            pQueueList = pQueueList->pNext;
        }
        pAgentList = pAgentList->pNext;
    }
    return nullptr;
}

void RemoveQueueFromList(uint64_t queueId)
{
    GPUAgentInfo *pAgentList = _r_rocm_debug_info.pAgentList;
    QueueInfo *pQueueList = nullptr;
    while (pAgentList != nullptr)
    {
        pQueueList = pAgentList->pQueueList;
        while (pQueueList != nullptr)
        {
            if (pQueueList->queueId == queueId)
            {
                goto FinishSearch;
            }
            pQueueList = pQueueList->pNext;
        }
        pAgentList = pAgentList->pNext;
    }

FinishSearch:
    if (pQueueList == nullptr)
    {
        AGENT_ERROR("Unable to delete queue in _r_rocm_debug_info: can not find queue with ID" << queueId);
        return;
    }
    else
    {
        if (pQueueList->pPrev != nullptr)
        {
            pQueueList->pPrev->pNext = pQueueList->pNext;
        }
        else
        {
            pAgentList->pQueueList = pQueueList->pNext;
        }

        if (pQueueList->pNext != nullptr)
        {
            pQueueList->pNext->pPrev = pQueueList->pPrev;
        }

        delete pQueueList;
    }
}


DebugAgentStatus AddExecutableToList(ExecutableInfo *pExec)
{
    DebugAgentStatus agentStatus = DEBUG_AGENT_STATUS_SUCCESS;
    agentStatus = AddToLinkListEnd<ExecutableInfo>(pExec, &(_r_rocm_debug_info.pExecutableList));
    if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Cannot add executable info to link list");
        return agentStatus;
    }

    return agentStatus;
}

DebugAgentStatus AddCodeObjectToList(CodeObjectInfo *pCodeObject, ExecutableInfo *pExecutable)
{
    // Create temp file for the loaded code object
    std::string codeObjPath;
    char sessionID[64];
    DebugAgentStatus agentStatus = DEBUG_AGENT_STATUS_SUCCESS;

    agentStatus = AgentGetDebugSessionID(&sessionID[0]);
    if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Cannot get debug session id");
        return agentStatus;
    }

    codeObjPath = g_codeObjDir;
    codeObjPath += "/ROCm_CodeObject_";
    codeObjPath += std::to_string(gs_numCodeObject);
    strncpy(&(pCodeObject->path[0]), codeObjPath.c_str(), sizeof(pCodeObject->path) - 1);
    pCodeObject->path[AGENT_MAX_FILE_PATH_LEN - 1] = '\0';

    agentStatus = AddToLinkListEnd<CodeObjectInfo>(pCodeObject, &(pExecutable->pCodeObjectList));
    if (agentStatus != DEBUG_AGENT_STATUS_SUCCESS)
    {
        AGENT_ERROR("Cannot add code object info to link list");
        return agentStatus;
    }

    gs_numCodeObject++;

    return agentStatus;
}

ExecutableInfo *GetExecutableFromList(uint64_t execId)
{
    ExecutableInfo *pListCurrent = _r_rocm_debug_info.pExecutableList;
    while (pListCurrent != nullptr)
    {
        if (pListCurrent->executableId == execId)
        {
            return pListCurrent;
        }
        pListCurrent = pListCurrent->pNext;
    }
    return nullptr;
}

void DeleteExecutableFromList(uint64_t execId)
{
    ExecutableInfo *pListCurrent = _r_rocm_debug_info.pExecutableList;
    while (pListCurrent != nullptr)
    {
        if (pListCurrent->executableId == execId)
        {
            break;
        }
        else
        {
            pListCurrent = pListCurrent->pNext;
        }
    }

    if (pListCurrent == nullptr)
    {
        AGENT_ERROR("Unable to delete executable in _r_rocm_debug_info: executable not found");
        return;
    }
    else
    {
        if (pListCurrent->pPrev != nullptr)
        {
            pListCurrent->pPrev->pNext = pListCurrent->pNext;
        }
        else
        {
            _r_rocm_debug_info.pExecutableList = pListCurrent->pNext;
        }

        if (pListCurrent->pNext != nullptr)
        {
            pListCurrent->pNext->pPrev = pListCurrent->pPrev;
        }

        /* delete all code objects of the executable */

        CodeObjectInfo *pCodeObject = pListCurrent->pCodeObjectList;
        CodeObjectInfo *pCodeObjectNext = nullptr;
        while (pCodeObject != nullptr)
        {
            pCodeObjectNext = pCodeObject->pNext;
            DeleteCodeObjectFromList(pCodeObject->addrLoaded, pListCurrent);
            pCodeObject = pCodeObjectNext;
        }

        delete pListCurrent;
    }
}


void DeleteCodeObjectFromList(uint64_t addrLoaded, ExecutableInfo *pExecutable)
{
    CodeObjectInfo *pListCurrent = pExecutable->pCodeObjectList;
    while (pListCurrent != nullptr)
    {
        if (pListCurrent->addrLoaded == addrLoaded)
        {
            break;
        }
        else
        {
            pListCurrent = pListCurrent->pNext;
        }
    }

    if (pListCurrent == nullptr)
    {
        AGENT_ERROR("Unable to delete code object in _r_rocm_debug_info: code object not found");
        return;
    }
    else
    {
        if (pListCurrent->pPrev != nullptr)
        {
            pListCurrent->pPrev->pNext = pListCurrent->pNext;
        }
        else
        {
            pExecutable->pCodeObjectList = pListCurrent->pNext;
        }

        if (pListCurrent->pNext != nullptr)
        {
            pListCurrent->pNext->pPrev = pListCurrent->pPrev;
        }

        if (g_deleteTmpFile)
        {
            AgentDeleteFile(pListCurrent->path);
        }

        delete[] (char*) pListCurrent->addrMemory;
        delete pListCurrent;
    }
}
