// license:BSD-3-Clause
// copyright-holders:Ville Linde
//
// The SHARC's start, reset, flag inputs and host boot, from MAME's sharc.cpp
// (device_start, device_reset in BOOT_MODE_HOST, set_flag_input,
// external_dma_write) at dddd73680656e355bb2b5beecab1167c9f07bf81. See
// sharc.h and THIRD_PARTY.md.

#include "runtime/sharc/sharc_impl.h"

namespace rt::sharc {

#include "runtime/sharc/mame/tables.inc"
#include "runtime/sharc/mame/boot.inc"

Sharc::Sharc() : m_heap_core(std::make_unique<sharc_internal_state>()) {
    // device_start: the interpreter's state starts zeroed.
    m_core = m_heap_core.get();
    std::memset(m_core, 0, sizeof(sharc_internal_state));
    m_core->fp_const.k0_0 = 0.0F;
    m_core->fp_const.k0_5 = 0.5F;
    m_core->fp_const.k1_0 = 1.0F;
    m_core->fp_const.k2_0 = 2.0F;
    std::memset(m_blocks, 0, sizeof m_blocks);
}

void Sharc::reset() {
    std::memset(m_blocks, 0, sizeof m_blocks);

    m_core->dma[6].int_index = 0x20000;
    m_core->dma[6].int_modifier = 1;
    m_core->dma[6].int_count = 0x100;
    m_core->dma[6].ext_index = 0x400000;
    m_core->dma[6].ext_modifier = 1;
    m_core->dma[6].ext_count = 0x600;
    m_core->dma[6].control = 0xa1;

    m_core->pc = 0x20004;
    m_core->extdma_shift = 0;
    m_core->daddr = m_core->pc + 1;
    m_core->faddr = m_core->daddr + 1;
    m_core->nfaddr = m_core->faddr + 1;

    m_core->idle = 0;
    m_core->mode1 = 0x00000000;
    m_core->mode2 &= 0xf0000000;
    m_core->astat &= FLG0 | FLG1 | FLG2 | FLG3;
    m_core->stky = PCEM | SSEM | LSEM;
    m_core->irptl = 0x0000;
    m_core->imask = 0x0003;
    m_core->ustat1 = 0x0000;
    m_core->ustat2 = 0x0000;

    m_core->pcstkp = 0;
    m_core->lstkp = 0;
    m_core->pcstk = 0x00ffffff;
    m_core->curlcntr = 0xffffffff;
    m_core->lcntr = m_core->lcstack[0];
    m_core->laddr.unpack(0xffffffff);
    m_core->status_stkp = 0;
    m_core->interrupt_active = 0;

    m_core->syscon = 0x00000010;
    m_core->sysstat &= 0x00000ff0;
    m_core->iop_write_num = 0;
    m_core->iop_data = 0;
}

void Sharc::set_flag_input(int flag_num, int state) {
    if (BIT(m_core->mode2, flag_num + 15)) throw SharcFatal("sharc_set_flag_input: flag %d is set output!", flag_num);
    m_core->flag[flag_num] = state ? 1 : 0;
}

} // namespace rt::sharc
