// v60::Interp: the hooks that run MAME's own operand decoders and
// addressing modes at run time, so v60::Core's op handlers execute exactly
// as MAME's V60 does. This is the reference interpreter: the trace harness
// (m1replay) and the recompiler's unit tests use it, and the game runs code
// the recompiler did not reach through it (Board::FallbackEnv).
#pragma once

#include "v60/core.h"

namespace v60 {

struct Interp {
    static Core::am_func fn(Am k);
    static void enter(Core &, const char *) {}
    static uint32_t ReadAM(Core &c) { return c.ReadAM(); }
    static uint32_t BitReadAM(Core &c) { return c.BitReadAM(); }
    static uint32_t ReadAMAddress(Core &c) { return c.ReadAMAddress(); }
    static uint32_t BitReadAMAddress(Core &c) { return c.BitReadAMAddress(); }
    static uint32_t WriteAM(Core &c) { return c.WriteAM(); }
    static void F12DecodeFirstOperand(Core &c, Am k1, uint8_t d1) { c.F12DecodeFirstOperand(fn(k1), d1); }
    static void F12WriteSecondOperand(Core &c, uint8_t d2) { c.F12WriteSecondOperand(d2); }
    static void F12DecodeOperands(Core &c, Am k1, uint8_t d1, Am k2, uint8_t d2) {
        c.F12DecodeOperands(fn(k1), d1, fn(k2), d2);
    }
    static void F2DecodeFirstOperand(Core &c, Am k1, uint8_t d1) { c.F2DecodeFirstOperand(fn(k1), d1); }
    static void F2DecodeSecondOperand(Core &c, Am k2, uint8_t d2) { c.F2DecodeSecondOperand(fn(k2), d2); }
    static void F2WriteSecondOperand(Core &c, uint8_t d2) { c.F2WriteSecondOperand(d2); }
    static void F7aDecodeOperands(Core &c, Am k1, uint8_t d1, Am k2, uint8_t d2) {
        c.F7aDecodeOperands(fn(k1), d1, fn(k2), d2);
    }
    static void F7bDecodeFirstOperand(Core &c, Am k1, uint8_t d1) { c.F7bDecodeFirstOperand(fn(k1), d1); }
    static void F7bWriteSecondOperand(Core &c, uint8_t d2) { c.F7bWriteSecondOperand(d2); }
    static void F7bDecodeOperands(Core &c, Am k1, uint8_t d1, Am k2, uint8_t d2) {
        c.F7bDecodeOperands(fn(k1), d1, fn(k2), d2);
    }
    static void F7cDecodeOperands(Core &c, Am k1, uint8_t d1, Am k2, uint8_t d2) {
        c.F7cDecodeOperands(fn(k1), d1, fn(k2), d2);
    }

    // One instruction, as MAME's execute_run does it: PC advanced by the
    // handler's length. (Nothing here stalls: devices answer at once.)
    static void step(Core &c);
};

} // namespace v60
