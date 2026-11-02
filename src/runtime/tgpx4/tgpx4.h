// license:BSD-3-Clause
// copyright-holders:Angelo Salese, ElSemi, Matthew Daniels
//
// The Model 2C geometry processor: a Fujitsu MB86235 "TGPx4". Its semantics
// are MAME's (src/devices/cpu/mb86235 at
// dddd73680656e355bb2b5beecab1167c9f07bf81, BSD-3-Clause; notice above kept
// as the licence requires), transplanted by scripts/transplant_tgpx4.py into
// mame/*.inc around this class, which replaces MAME's device framework: the
// two internal RAMs are arrays, the FIFOs and the external bus are
// Tgpx4Bus. A pop from the empty input FIFO stalls the instruction as
// MAME's generic_fifo does (it reads 0 and is run again once there is a
// word). The program the i960 uploads is statically recompiled by
// tools/m2tgpx4recomp, whose code runs MAME's execute_run step around each
// instruction with its opcode as a constant; nothing here runs an
// instruction from memory. See THIRD_PARTY.md.
#pragma once

#include <cmath>
#include <math.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace rt::tgpx4 {

// MAME's fatalerror. The printf format is kept as the message, followed by
// the arguments in hex.
struct Tgpx4Fatal : std::runtime_error {
    template <typename... A> explicit Tgpx4Fatal(const char *fmt, A... a) : std::runtime_error(message(fmt, a...)) {}

private:
    template <typename T> static std::string arg(T v) {
        char b[24];
        std::snprintf(b, sizeof b, "%llx", (unsigned long long)(uint64_t)v);
        return b;
    }
    template <typename... A> static std::string message(const char *fmt, A... a) {
        std::string m = fmt;
        while (!m.empty() && m.back() == '\n') m.pop_back();
        ((m += " [" + arg(a) + "]"), ...);
        return m;
    }
};

// MAME's lib/util helpers the interpreter uses.
using u32 = uint32_t;
template <typename T, typename U> constexpr T BIT(T x, U n) noexcept { return (x >> n) & T(1); }
inline float u2f(uint32_t v) {
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}
inline uint32_t f2u(float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    return v;
}
// int(float) and uint32_t(float) as MAME's x86-64 build converts them
// (cvttss2si, 32- and 64-bit): out of range and NaN give INT32_MIN, and the
// unsigned conversion is the low half of the 64-bit one.
inline int32_t x86_f2i(float f) {
    if (!(f >= -2147483648.0f && f < 2147483648.0f)) return INT32_MIN;
    return int32_t(f);
}
inline uint32_t x86_f2u(float f) {
    if (!(f >= -9223372036854775808.0f && f < 9223372036854775808.0f)) return 0;
    return uint32_t(uint64_t(int64_t(f)));
}

// The board's side: the FIFOs (MAME generic_fifo_u32) and the external bus
// (the data address space, 24-bit dword addresses: copro_tgpx4_data_map).
struct Tgpx4Bus {
    virtual bool in_empty() = 0;
    virtual bool in_full() = 0;
    virtual uint32_t in_pop() = 0; // not empty
    virtual void in_clear() = 0;
    virtual bool out_empty() = 0;
    virtual bool out_full() = 0;
    virtual void out_push(uint32_t v) = 0;
    virtual void out_clear() = 0;
    virtual uint32_t ext_r(uint32_t address) = 0;
    virtual void ext_w(uint32_t address, uint32_t data) = 0;
};

class Tgpx4;
using mb86235_device = Tgpx4;

class Tgpx4 {
public:
    Tgpx4() : m_core(&m_state) { std::memset(&m_state, 0, sizeof m_state); }
    Tgpx4(const Tgpx4 &) = delete;
    Tgpx4 &operator=(const Tgpx4 &) = delete;

    // MAME device_reset (the internal RAMs keep their contents, as MAME's).
    void reset() {
        m_core->pc = 0;
        m_core->delay_pc = 0;
        m_core->ppc = 0;
        m_core->delay_slot = false;
    }
    // MAME stall(): the input FIFO was popped while empty.
    void stall() { m_core->cur_fifo_state.has_stalled = true; }

    Tgpx4Bus *bus = nullptr;
    uint64_t count = 0; // instructions executed

    // MAME execute_run, around one instruction: the address it runs from...
    uint32_t next_pc() { return check_previous_op_stall() ? m_core->cur_fifo_state.pc : m_core->pc; }
    // ...and the step before execute_op.
    void step(uint32_t curpc) {
        m_core->ppc = curpc;
        if (m_core->delay_slot == true) {
            m_core->pc = m_core->delay_pc;
            m_core->delay_slot = false;
        } else
            handle_single_step_execution();
        m_core->cur_fifo_state.has_stalled = false;
        m_core->icount = 0; // MAME's cycle budget: the recompiled program counts instructions
    }

#include "runtime/tgpx4/mame/decls.inc"

    // Ours: what the device framework did.
    struct Fifo {
        Tgpx4 *t;
        bool in;
        explicit operator bool() const { return true; }
        Fifo *operator->() { return this; }
        uint32_t pop() { // generic_fifo pop: an empty FIFO stalls the reader, which reads 0
            if (t->bus->in_empty()) {
                t->stall();
                return 0;
            }
            return t->bus->in_pop();
        }
        void push(uint32_t v) { t->bus->out_push(v); }
        bool is_empty() { return in ? t->bus->in_empty() : t->bus->out_empty(); }
        bool is_full() { return in ? t->bus->in_full() : t->bus->out_full(); }
        void clear() {
            if (in)
                t->bus->in_clear();
            else
                t->bus->out_clear();
        }
    };
    struct External {
        Tgpx4 *t;
        uint32_t read_dword(uint32_t address) { return t->bus->ext_r(address & 0xffffff); }
        void write_dword(uint32_t address, uint32_t data) { t->bus->ext_w(address & 0xffffff, data); }
    };
    struct Internal { // internal_abus / internal_bbus: 0x400 dwords of RAM
        uint32_t ram[0x400];
        uint32_t read_dword(uint32_t address) const { return ram[address & 0x3ff]; }
        void write_dword(uint32_t address, uint32_t data) { ram[address & 0x3ff] = data; }
    };

    mb86235_internal_state m_state; // m_core (decls.inc) points here
    Fifo m_fifoin{this, true}, m_fifoout{this, false};
    External m_external{this};
    Internal m_dataa{}, m_datab{};
};

} // namespace rt::tgpx4
