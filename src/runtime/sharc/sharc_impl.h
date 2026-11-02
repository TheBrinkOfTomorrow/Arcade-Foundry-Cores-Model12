// license:BSD-3-Clause
// copyright-holders:Ville Linde
//
// The SHARC's semantics, for the recompiled program and src/runtime/sharc:
// MAME's handlers (mame/*.inc) and, ours, the address spaces MAME's maps
// described (sharc.cpp pgm_2m and data_2m: the ADSP-21062's two 2-Mbit
// blocks, the IOP registers, the external port) and the loop test of MAME's
// execute_run, which the recompiled code runs at each loop end address.
#pragma once

#include "runtime/sharc/sharc.h"

namespace rt::sharc {

#include "runtime/sharc/mame/device.inc"
#include "runtime/sharc/mame/ops.inc"

// pgm_2m: block 0 at 0x20000-0x24fff, block 1 mirrored at 0x28000, 0x30000
// and 0x38000. The rest of the 24-bit space is unmapped (reads 0).
inline uint64_t Sharc::ProgramSpace::read_qword(uint32_t address) {
    address &= 0xffffff;
    if (address < 0x20000 || address >= 0x40000 || (address & 0x7fff) >= 0x5000) return 0;
    return (address & 0x18000) ? s->pm_r<1>(address & 0x7fff) : s->pm_r<0>(address & 0x7fff);
}

inline void Sharc::ProgramSpace::write_qword(uint32_t address, uint64_t data, uint64_t mem_mask) {
    address &= 0xffffff;
    if (address < 0x20000 || address >= 0x40000 || (address & 0x7fff) >= 0x5000) return;
    if (address & 0x18000)
        s->pm_w<1>(address & 0x7fff, data, mem_mask);
    else
        s->pm_w<0>(address & 0x7fff, data, mem_mask);
}

// data_2m: the IOP registers at 0-0xff; block 0 at 0x20000-0x27fff (32-bit
// words), block 1 mirrored at 0x28000-0x3ffff; short words of block 0 at
// 0x40000-0x4ffff and of block 1 mirrored at 0x50000-0x7ffff; above
// 0x400000 the external port (the board's map).
inline uint32_t Sharc::DataSpace::read_dword(uint32_t address) {
    if (address >= 0x400000) return s->bus->ext_r(address);
    if (address < 0x100) return s->iop_r(address);
    if (address < 0x20000) return 0;
    if (address < 0x40000) return s->m_blocks[(address & 0x18000) ? 1 : 0][address & 0x7fff];
    if (address < 0x80000) {
        const bool b1 = (address & 0x30000) != 0;
        if (s->m_dm_short_se) return b1 ? s->dm_short_se_r<1>(address & 0xffff) : s->dm_short_se_r<0>(address & 0xffff);
        return b1 ? s->dm_short_r<1>(address & 0xffff) : s->dm_short_r<0>(address & 0xffff);
    }
    return 0;
}

inline void Sharc::DataSpace::write_dword(uint32_t address, uint32_t data) {
    if (address >= 0x400000) return s->bus->ext_w(address, data);
    if (address < 0x100) return s->iop_w(address, data);
    if (address < 0x20000) return;
    if (address < 0x40000) {
        s->m_blocks[(address & 0x18000) ? 1 : 0][address & 0x7fff] = data;
        return;
    }
    if (address < 0x80000) {
        if (address & 0x30000)
            s->dm_short_w<1>(address & 0xffff, data);
        else
            s->dm_short_w<0>(address & 0xffff, data);
    }
}

// MAME's DMA runs on timers; Model 2B's program uses none after the boot.
inline void Sharc::sharc_iop_delayed_w(uint32_t reg, uint32_t, int) {
    if (held && reg == 0x1c) return; // Last Bronx's upload through EPB0 (see held)
    throw SharcFatal("SHARC: DMA started (IOP register %02X): not supported", reg);
}

inline void Sharc::dma_run_cycle(int channel) {
    if (m_core->dma_status & (1 << channel)) throw SharcFatal("SHARC: DMA running on channel %d: not supported", channel);
}

// MAME execute_run: "handle looping", at the instruction whose address is
// the innermost loop's end (the recompiler emits the call only at addresses
// a DO instruction names as a loop end).
inline void Sharc::loop_check() {
    if (!(m_core->stky & LSEM) && (m_core->pc == m_core->laddr.addr)) {
        switch (m_core->laddr.loop_type) {
        case 0: // arithmetic condition-based
        {
            if ((m_core->pc - TOP_PC()) > 2) m_core->astat = m_core->astat_old_old_old;

            if (DO_CONDITION_CODE(m_core->laddr.code)) {
                POP_LOOP();
                POP_PC();
            } else {
                CHANGE_PC(TOP_PC());
            }

            m_core->astat = m_core->astat_old;
            break;
        }
        case 1: // counter-based, length 1
        case 2: // counter-based, length 2
        case 3: // counter-based, length >2
        {
            --m_core->curlcntr;
            if (m_core->curlcntr == 0) {
                POP_LOOP();
                POP_PC();
            } else {
                CHANGE_PC(TOP_PC());
            }
        }
        }
    }
}

} // namespace rt::sharc
