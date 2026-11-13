// v60::Core: the NEC V60 (uPD70615) state and instruction semantics, as
// MAME's executor computes them (src/v60/mame, transplanted BSD-3; MAME is
// the oracle, design doc "Model 1", rules.md 9).
//
// The op handlers are MAME's, templated on a hook class H: they call H for
// every operand decode and addressing mode (H::F12DecodeOperands,
// H::ReadAM, ...). Two hook classes exist:
//
//   v60::Interp (src/v60/interp.h)  MAME's own decoders, run at run time:
//       the reference interpreter for the trace harness and unit tests.
//   generated (tools/m1recomp)      one per instruction, every addressing
//       mode resolved at recompile time to plain loads, stores and
//       register updates; the handler body is the only thing left to run.
//
// Memory goes through v60::Space: RAM and ROM are 64 KiB host pages read
// inline, everything else is a device (Bus::dev_read / dev_write), at the
// granularity the CPU accesses it (MAME's executor's read_byte /
// read_word_unaligned / read_dword_unaligned calls), with 24-bit addresses.
#pragma once

#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace v60 {

struct Fatal : std::runtime_error {
    using std::runtime_error::runtime_error;
};

[[noreturn]] inline void v60_fatal(const char *fmt, ...) {
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    throw Fatal(b);
}
inline void v60_log(const char *, ...) {}

enum class Am : uint8_t { Read, BitRead, Addr, BitAddr, Write };

// The board's memory: 256 pages of 64 KiB over the V60's 24-bit bus.
// rd[p] / wr[p] point at host memory for RAM and ROM (wr null for ROM:
// writes are dropped, as MAME drops them); a page with rd null is a device.
struct Bus {
    virtual ~Bus() = default;
    uint8_t *rd[256] = {};
    uint8_t *wr[256] = {};
    // size 1, 2, 4; space 0 program, 1 I/O. Unaligned accesses arrive whole.
    virtual uint32_t dev_read(uint32_t a, unsigned size, unsigned space) = 0;
    virtual void dev_write(uint32_t a, uint32_t v, unsigned size, unsigned space) = 0;
};

class Space {
public:
    Bus *bus = nullptr;
    unsigned space = 0;

    // Instruction fetch: always RAM or ROM (MAME's opcode cache).
    uint8_t op_read8(uint32_t a) const { return mem8(a); }
    uint16_t op_read16(uint32_t a) const { return uint16_t(mem8(a) | mem8(a + 1) << 8); }
    uint32_t op_read32(uint32_t a) const { return uint32_t(op_read16(a)) | uint32_t(op_read16(a + 2)) << 16; }

    uint8_t read_byte(uint32_t a) {
        a &= 0xffffff;
        if (space == 0)
            if (const uint8_t *p = bus->rd[a >> 16]) return p[a & 0xffff];
        return uint8_t(bus->dev_read(a, 1, space));
    }
    uint16_t read_word_unaligned(uint32_t a) {
        a &= 0xffffff;
        if (space == 0 && (a & 0xffff) <= 0xfffe)
            if (const uint8_t *p = bus->rd[a >> 16]) {
                uint16_t v;
                std::memcpy(&v, p + (a & 0xffff), 2);
                return v;
            }
        if (space == 0 && bus->rd[a >> 16] && bus->rd[((a + 1) & 0xffffff) >> 16])
            return uint16_t(mem8(a) | mem8(a + 1) << 8);
        return uint16_t(bus->dev_read(a, 2, space));
    }
    uint32_t read_dword_unaligned(uint32_t a) {
        a &= 0xffffff;
        if (space == 0 && (a & 0xffff) <= 0xfffc)
            if (const uint8_t *p = bus->rd[a >> 16]) {
                uint32_t v;
                std::memcpy(&v, p + (a & 0xffff), 4);
                return v;
            }
        if (space == 0 && bus->rd[a >> 16] && bus->rd[((a + 3) & 0xffffff) >> 16])
            return uint32_t(mem8(a) | mem8(a + 1) << 8 | mem8(a + 2) << 16) | uint32_t(mem8(a + 3)) << 24;
        return bus->dev_read(a, 4, space);
    }
    uint32_t read_dword(uint32_t a) { return read_dword_unaligned(a); }
    uint64_t read_qword_unaligned(uint32_t a) {
        const uint32_t lo = read_dword_unaligned(a);
        return uint64_t(read_dword_unaligned(a + 4)) << 32 | lo;
    }
    void write_byte(uint32_t a, uint8_t v) {
        a &= 0xffffff;
        if (space == 0 && bus->rd[a >> 16]) {
            if (uint8_t *p = bus->wr[a >> 16]) p[a & 0xffff] = v;
            return;
        }
        bus->dev_write(a, v, 1, space);
    }
    void write_word_unaligned(uint32_t a, uint16_t v) {
        a &= 0xffffff;
        if (space == 0 && bus->rd[a >> 16]) {
            if ((a & 0xffff) <= 0xfffe) {
                if (uint8_t *p = bus->wr[a >> 16]) std::memcpy(p + (a & 0xffff), &v, 2);
            } else {
                put8(a, uint8_t(v));
                put8(a + 1, uint8_t(v >> 8));
            }
            return;
        }
        bus->dev_write(a, v, 2, space);
    }
    void write_dword_unaligned(uint32_t a, uint32_t v) {
        a &= 0xffffff;
        if (space == 0 && bus->rd[a >> 16]) {
            if ((a & 0xffff) <= 0xfffc) {
                if (uint8_t *p = bus->wr[a >> 16]) std::memcpy(p + (a & 0xffff), &v, 4);
            } else {
                for (int i = 0; i < 4; i++) put8(a + i, uint8_t(v >> 8 * i));
            }
            return;
        }
        bus->dev_write(a, v, 4, space);
    }
    void write_qword_unaligned(uint32_t a, uint64_t v) {
        write_dword_unaligned(a, uint32_t(v));
        write_dword_unaligned(a + 4, uint32_t(v >> 32));
    }

private:
    uint8_t mem8(uint32_t a) const {
        a &= 0xffffff;
        const uint8_t *p = bus->rd[a >> 16];
        return p ? p[a & 0xffff] : 0xff;
    }
    void put8(uint32_t a, uint8_t v) {
        a &= 0xffffff;
        if (uint8_t *p = bus->wr[a >> 16]) p[a & 0xffff] = v;
    }
};

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
inline int64_t mul_32x32(int32_t a, int32_t b) { return int64_t(a) * int64_t(b); }
inline uint64_t mulu_32x32(uint32_t a, uint32_t b) { return uint64_t(a) * uint64_t(b); }
inline int32_t div_64x32_rem(int64_t a, int32_t b, int32_t &rem) {
    const int64_t q = a / int64_t(b);
    rem = int32_t(a % int64_t(b));
    return int32_t(q);
}
inline uint32_t divu_64x32_rem(uint64_t a, uint32_t b, uint32_t &rem) {
    const uint64_t q = a / uint64_t(b);
    rem = uint32_t(a % uint64_t(b));
    return uint32_t(q);
}
template <int N, typename T> inline T bitswap(T v, int b0, int b1, int b2, int b3, int b4, int b5, int b6, int b7) {
    static_assert(N == 8, "bitswap<8> only");
    const int b[8] = {b0, b1, b2, b3, b4, b5, b6, b7};
    T r = 0;
    for (int i = 0; i < 8; i++) r = T(r | (((v >> b[i]) & 1) << (7 - i)));
    return r;
}
inline uint32_t swapendian_int32(uint32_t v) {
    return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
}

template <class H> struct Tables;

class Core {
public:
    typedef uint32_t (Core::*am_func)();
    typedef uint32_t (Core::*op6_func)(int reg);

    // MAME's v60_device state, same names (the transplanted code uses them)
    uint32_t m_reg[68] = {};
    struct {
        uint8_t CY, OV, S, Z;
    } m_flags = {};
    Space m_program_space, m_io_space;
    Space *m_program = &m_program_space;
    Space *m_io = &m_io_space;
    uint32_t m_PPC = 0;
    int m_stall_io = 0;

    uint32_t m_op1 = 0, m_op2 = 0;
    uint8_t m_flag1 = 0, m_flag2 = 0;
    uint8_t m_instflags = 0;
    uint32_t m_lenop1 = 0, m_lenop2 = 0;
    uint8_t m_subop = 0;
    uint32_t m_bamoffset1 = 0, m_bamoffset2 = 0;
    uint8_t m_amflag = 0;
    uint32_t m_amout = 0;
    uint32_t m_bamoffset = 0;
    uint32_t m_amlength1 = 0, m_amlength2 = 0;
    uint32_t m_modadd = 0;
    uint8_t m_modm = 0;
    uint8_t m_modval = 0;
    uint8_t m_modval2 = 0;
    uint8_t m_modwritevalb = 0;
    uint16_t m_modwritevalh = 0;
    uint32_t m_modwritevalw = 0;
    uint8_t m_moddim = 0;

    explicit Core(Bus *bus) {
        m_program_space.bus = bus;
        m_program_space.space = 0;
        m_io_space.bus = bus;
        m_io_space.space = 1;
        m_reg[45] = 0x00006000; // PIR (V60)
    }

    // MAME's device_reset.
    void reset() {
        m_reg[33] = 0x10000000; // PSW
        m_reg[32] = 0xfffffff0; // PC
        m_reg[41] = 0;          // SBR
        m_reg[43] = 0x00000070; // SYCW
        m_reg[44] = 0x0000e000; // TKCW
        m_reg[51] = 0x0000f002; // PSW2
        m_flags = {};
    }

    // Save states, between instructions: the registers and flags (the rest
    // is an instruction's scratch).
    template <typename S> void state(S &s) { s(m_reg), s(m_flags), s(m_PPC), s(m_stall_io); }
    uint32_t pc() const { return m_reg[32]; }
    void set_pc(uint32_t v) { m_reg[32] = v; }
    // PSW with the flags folded in (MAME's v60ReadPSW, without its side effect)
    uint32_t psw() const {
        return (m_reg[33] & 0xfffffff0) | (m_flags.Z ? 1 : 0) | (m_flags.S ? 2 : 0) | (m_flags.OV ? 4 : 0) |
               (m_flags.CY ? 8 : 0);
    }
    bool interrupts_enabled() const { return (m_reg[33] & (1u << 18)) != 0; }

    // MAME's v60_do_irq for a maskable interrupt (vector = irq_callback + 0x40),
    // without standard_irq_callback.
    void take_interrupt(int vector);

#include "v60/mame/macros.inc"

    void v60SaveStack() {
        if (PSW & 0x10000000)
            ISP = SP;
        else
            m_reg[37 + ((PSW >> 24) & 3)] = SP;
    }
    void v60ReloadStack() {
        if (PSW & 0x10000000)
            SP = ISP;
        else
            SP = m_reg[37 + ((PSW >> 24) & 3)];
    }
    uint32_t v60ReadPSW() {
        PSW &= 0xfffffff0;
        PSW |= (_Z ? 1 : 0) | (_S ? 2 : 0) | (_OV ? 4 : 0) | (_CY ? 8 : 0);
        return PSW;
    }
    void v60WritePSW(uint32_t newval) {
        int updateStack = 0;
        if ((newval ^ PSW) & 0x10000000)
            updateStack = 1;
        else if (!(PSW & 0x10000000) && ((newval ^ PSW) & 0x03000000))
            updateStack = 1;
        if (updateStack) v60SaveStack();
        PSW = newval;
        _Z = (uint8_t)(PSW & 1);
        _S = (uint8_t)(PSW & 2);
        _OV = (uint8_t)(PSW & 4);
        _CY = (uint8_t)(PSW & 8);
        if (updateStack) v60ReloadStack();
    }
    uint32_t v60_update_psw_for_exception(int is_interrupt, int target_level) {
        uint32_t oldPSW = v60ReadPSW();
        uint32_t newPSW = oldPSW;
        newPSW &= ~(3 << 24);
        newPSW |= target_level << 24;
        newPSW &= ~(1 << 18);
        newPSW &= ~(1 << 16);
        newPSW &= ~(1 << 27);
        newPSW &= ~(1 << 17);
        newPSW &= ~(1 << 29);
        if (is_interrupt) newPSW |= (1 << 28);
        newPSW |= (1u << 31);
        v60WritePSW(newPSW);
        return oldPSW;
    }

    // The interpreter's addressing modes and operand decoders (src/v60/interp.cpp)
#include "v60/mame/am_decls.inc"

    // The op handlers
#include "v60/mame/ops.inc"

#include "v60/mame/undef.inc"
};

inline void Core::take_interrupt(int vector) {
    uint32_t oldPSW = v60_update_psw_for_exception(1, 0);
    m_reg[31] -= 4;
    m_program->write_dword_unaligned(m_reg[31], oldPSW);
    m_reg[31] -= 4;
    m_program->write_dword_unaligned(m_reg[31], m_reg[32]);
    m_reg[32] = m_program->read_dword((m_reg[41] & ~0xfffu) + uint32_t(vector) * 4);
}

#include "v60/mame/tables.inc"

} // namespace v60
