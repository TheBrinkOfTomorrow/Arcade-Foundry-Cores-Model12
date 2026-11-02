#include "v60/interp.h"

namespace v60 {

#include "v60/mame/macros.inc"
#include "v60/mame/am.inc"
#include "v60/mame/undef.inc"

Core::am_func Interp::fn(Am k) {
    switch (k) {
    case Am::Read: return &Core::ReadAM;
    case Am::BitRead: return &Core::BitReadAM;
    case Am::Addr: return &Core::ReadAMAddress;
    case Am::BitAddr: return &Core::BitReadAMAddress;
    case Am::Write: return &Core::WriteAM;
    }
    return nullptr;
}

void Interp::step(Core &c) {
    c.m_PPC = c.m_reg[32];
    c.m_stall_io = 0;
    const uint32_t op = c.m_program->op_read8(c.m_reg[32]);
    c.m_reg[32] += (c.*Tables<Interp>::s_OpCodeTable[op])();
}

} // namespace v60
