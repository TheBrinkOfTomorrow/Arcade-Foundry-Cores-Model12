// The Digital Sound Board's Z80 program, statically recompiled
// (tools/dsbrecomp into gen/<set>_dsb/dsb_gen.cpp): its registers, memory
// and the board's I/O as the generated code sees them. The Z80 runs at
// 4 MHz; time is counted in T-states.
#pragma once

#include <cstdint>

namespace dsb {

struct Io {
    virtual ~Io() = default;
    // The Z80's I/O space (8-bit ports); `cycles` is the T-state the access
    // completes on.
    virtual uint8_t in(uint8_t port, uint64_t cycles) = 0;
    virtual void out(uint8_t port, uint8_t value, uint64_t cycles) = 0;
    // The INT line at T-state `cycles` (the board's i8251 RxRDY).
    virtual bool irq(uint64_t cycles) = 0;
};

struct Z80 {
    uint8_t a = 0, f = 0x40, b = 0, c = 0, d = 0, e = 0, h = 0, l = 0; // MAME device_start: Z set
    uint8_t a2 = 0, f2 = 0, b2 = 0, c2 = 0, d2 = 0, e2 = 0, h2 = 0, l2 = 0; // the alternate set
    uint16_t ix = 0xffff, iy = 0xffff, sp = 0, pc = 0;
    bool iff1 = false, iff2 = false, after_ei = false; // after_ei: no interrupt before the next instruction
    uint8_t im = 0;
    uint64_t cycles = 0;
    uint8_t mem[0x10000] = {}; // 0000-7FFF the program ROM, 8000-FFFF RAM
    Io *io = nullptr;
};

// Runs from z.pc until z.cycles reaches `until` (finishing the instruction
// under way). Interrupts (IM 1) are taken between instructions. Throws
// std::runtime_error on a jump into code the recompiler did not see.
void run(Z80 &z, uint64_t until);

} // namespace dsb
