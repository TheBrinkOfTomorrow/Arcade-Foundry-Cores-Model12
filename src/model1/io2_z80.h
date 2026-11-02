// The model1io2 I/O board's Z80 (a TMPZ84C015 at 9.8304 MHz) running its
// BIOS, statically recompiled (tools/dsbrecomp --io2 into
// gen/<set>_io/io_gen.cpp): its registers and the board as the generated
// code sees it. Time is counted in T-states; every access to the board names
// the T-state it happens on (MAME's Z80 does an M-cycle's access at its
// start).
#pragma once

#include <cstdint>

namespace io2 {

struct Io {
    virtual ~Io() = default;
    // Memory at 8000-DFFF (the 315-5338A, the board port, the DIP switches,
    // the ADC)
    virtual uint8_t mem_r(uint16_t a, uint64_t cycles) = 0;
    virtual void mem_w(uint16_t a, uint8_t v, uint64_t cycles) = 0;
    // The I/O space (the TMPZ84C015's CTC, SIO, PIO, watchdog)
    virtual uint8_t in(uint16_t port, uint64_t cycles) = 0;
    virtual void out(uint16_t port, uint8_t v, uint64_t cycles) = 0;
    // The INT line before an instruction starting at `cycles`; the
    // acknowledge (the daisy chain's vector) and RETI
    virtual bool irq(uint64_t cycles) = 0;
    virtual uint8_t irq_ack(uint64_t cycles) = 0;
    virtual void reti(uint64_t cycles) = 0;
};

struct Z80 {
    uint8_t a = 0xff, f = 0xff, b = 0, c = 0, d = 0, e = 0, h = 0, l = 0;
    uint8_t a2 = 0, f2 = 0, b2 = 0, c2 = 0, d2 = 0, e2 = 0, h2 = 0, l2 = 0;
    uint16_t ix = 0xffff, iy = 0xffff, sp = 0xffff, pc = 0;
    uint8_t i = 0, im = 0;
    bool iff1 = false, iff2 = false, after_ei = false;
    uint64_t cycles = 0, start = 0; // start: the T-state the instruction under way began on
    uint8_t mem[0x10000] = {};      // 0000-7FFF the BIOS, E000-FFFF RAM
    Io *io = nullptr;
};

// Runs from z.pc until the next instruction would start at `until` or later
// (or, for one that may touch the board, its access would). Throws
// std::runtime_error on a jump into code the recompiler did not see.
void run(Z80 &z, uint64_t until);

} // namespace io2
