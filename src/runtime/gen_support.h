// Support for recompiled i960 code (tools/m2recomp output). Generated code is
// native C++: every instruction's semantics are emitted inline with operands
// resolved at recompile time. It calls into the runtime only for memory
// access, call/return frame management and interrupt entry, as a CPU would.
#pragma once

#include "runtime/cpu.h"
#include "runtime/enhance.h"
#include "runtime/lockstep.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include "compat/bit_cast.h"
namespace gen {

struct Env {
    rt::Cpu &c;
    rt::Lockstep &ls;
};

// Condition codes, as MAME's cmp_u / cmp_s / concmp_u / concmp_s.
inline uint32_t cc_u(uint32_t a, uint32_t b) { return a < b ? 4u : a == b ? 2u : 1u; }
inline uint32_t cc_s(int32_t a, int32_t b) { return a < b ? 4u : a == b ? 2u : 1u; }
inline uint32_t cc_d(double a, double b) { return a < b ? 4u : a == b ? 2u : a > b ? 1u : 0u; }

// FP with MAME's host-double semantics (lockstep against MAME). The shipped
// path will use the proven native paths in i960/fp.h instead.
inline double round_to_int(double v, uint32_t ac) {
    switch ((ac >> 30) & 3) {
    case 0: return std::round(v);
    case 1: return std::floor(v);
    case 2: return std::ceil(v);
    default: return std::trunc(v);
    }
}
inline uint32_t f2u(float f) { return compat::bit_cast<uint32_t>(f); }

#ifdef M2_FAST_GEN
// Work RAM at a fixed offset, for the rewritten code (the Dreamcast's and the
// Vita's: platform/dreamcast/scripts/fast_gen.py): what the bus does for these
// addresses (plain RAM, little-endian, aligned), without the calls.
inline uint32_t wram_r32(const rt::Cpu &c, uint32_t o) { uint32_t v; std::memcpy(&v, M2_AL(c.work_ram + o, 4), 4); return v; }
inline uint16_t wram_r16(const rt::Cpu &c, uint32_t o) { uint16_t v; std::memcpy(&v, M2_AL(c.work_ram + o, 2), 2); return v; }
inline uint8_t wram_r8(const rt::Cpu &c, uint32_t o) { return c.work_ram[o]; }
inline void wram_w32(rt::Cpu &c, uint32_t o, uint32_t v) { std::memcpy(M2_AL(c.work_ram + o, 4), &v, 4); }
inline void wram_w16(rt::Cpu &c, uint32_t o, uint16_t v) { std::memcpy(M2_AL(c.work_ram + o, 2), &v, 2); }
inline void wram_w8(rt::Cpu &c, uint32_t o, uint8_t v) { c.work_ram[o] = v; }
#endif
inline float u2f(uint32_t u) { return compat::bit_cast<float>(u); }

// Real operands, as MAME's get_1_rif / get_2_rif / set_rif and their long
// forms. `op` is the instruction word, a constant in generated code, so the
// mode tests fold away. A literal is fp0-fp3, 1.0 (0x16) or 0.0.
// flushreg, as MAME: the cached frames go to memory, oldest first.
inline void flushreg(rt::Cpu &c) {
    if (c.m_rcache_pos > 4) c.m_rcache_pos = 4;
    for (int f = 0; f < c.m_rcache_pos; ++f)
        for (uint32_t i = 0; i < 0x10; ++i) c.bus->write_dword(c.m_rcache_frame_addr[f] + i * 4, c.m_rcache[f][i]);
    c.m_rcache_pos = 0;
}

inline double rif_lit(const rt::Cpu &c, uint32_t idx) { return idx < 4 ? c.m_fp[idx] : idx == 0x16 ? 1.0 : 0.0; }
inline double rif1(const rt::Cpu &c, uint32_t op) { return op & 0x800 ? rif_lit(c, op & 0x1f) : double(u2f(c.m_r[op & 0x1f])); }
inline double rif2(const rt::Cpu &c, uint32_t op) { return op & 0x1000 ? rif_lit(c, (op >> 14) & 0x1f) : double(u2f(c.m_r[(op >> 14) & 0x1f])); }
inline void set_rif(rt::Cpu &c, uint32_t op, double v) {
    if (!(op & 0x2000)) c.m_r[(op >> 19) & 0x1f] = f2u(float(v));
    else c.m_fp[(op >> 19) & 3] = v; // the recompiler rejects a literal other than fp0-fp3
}
inline double rifl_reg(const rt::Cpu &c, uint32_t n) { return rt::u2d(uint64_t(c.m_r[n]) | (uint64_t(c.m_r[n + 1]) << 32)); }
inline double rifl1(const rt::Cpu &c, uint32_t op) { return op & 0x800 ? rif_lit(c, op & 0x1f) : rifl_reg(c, op & 0x1e); }
inline double rifl2(const rt::Cpu &c, uint32_t op) { return op & 0x1000 ? rif_lit(c, (op >> 14) & 0x1f) : rifl_reg(c, (op >> 14) & 0x1e); }
inline void set_rifl(rt::Cpu &c, uint32_t op, double v) {
    if (!(op & 0x2000)) {
        const uint64_t u = rt::d2u(v);
        c.m_r[(op >> 19) & 0x1e] = uint32_t(u);
        c.m_r[((op >> 19) & 0x1e) + 1] = uint32_t(u >> 32);
    } else c.m_fp[(op >> 19) & 3] = v;
}

// movre, as MAME's (i960.cpp, with its double_to_extended and
// extended_to_double): an Extended real is three registers (fraction with
// an explicit integer bit in words 0/1, sign and 15-bit exponent in word 2,
// its upper 16 bits dropped) or fp0-fp3, held as doubles. The recompiler
// rejects a literal destination (MAME's fatalerror).
inline void double_to_extended(double val, uint32_t *words) {
    const uint64_t bits = rt::d2u(val);
    const uint32_t sign = uint32_t(bits >> 63) << 15;
    uint64_t frac;
    uint32_t e;
    if (std::isnan(val) || std::isinf(val)) {
        frac = (1ULL << 63) | ((bits & 0x000fffffffffffffULL) << 11);
        e = 0x7fff;
    } else if (val == 0.0) {
        frac = 0;
        e = 0;
    } else {
        int exp2;
        const double m = std::frexp(std::fabs(val), &exp2); // 0.5 <= m < 1, exact for denormals too
        frac = uint64_t(std::ldexp(m, 64));                 // integer bit lands in bit 63
        e = uint32_t(exp2 - 1 + 16383);
    }
    words[0] = uint32_t(frac);
    words[1] = uint32_t(frac >> 32);
    words[2] = sign | e;
}
inline double extended_to_double(const uint32_t *words) {
    const uint64_t frac = words[0] | (uint64_t(words[1]) << 32);
    const uint32_t e = words[2] & 0x7fff;
    double val;
    if (e == 0x7fff) val = !(frac << 1) ? std::numeric_limits<double>::infinity() // integer bit only: infinity
                                        : rt::u2d(0x7ff8000000000000ULL | ((frac & 0x7fffffffffffffffULL) >> 11)); // NaN, quiet
    else val = std::ldexp(double(frac), int(e ? e : 1) - 16383 - 63); // e == 0 is a denormal (exponent 1)
    return (words[2] >> 15) & 1 ? -val : val;
}
inline void movre(rt::Cpu &c, uint32_t op) {
    uint32_t v[3];
    if (!(op & 0x800)) { // three g/l registers (a multiple of 4)
        const uint32_t src = op & 0x1c;
        v[0] = c.m_r[src], v[1] = c.m_r[src + 1], v[2] = c.m_r[src + 2] & 0xffff;
    } else {
        const uint32_t idx = op & 0x1f;
        double_to_extended(idx < 4 ? c.m_fp[idx] : idx == 0x16 ? 1.0 : 0.0, v);
    }
    if (!(op & 0x2000)) {
        const uint32_t dst = (op >> 19) & 0x1c;
        c.m_r[dst] = v[0], c.m_r[dst + 1] = v[1], c.m_r[dst + 2] = v[2];
    } else c.m_fp[(op >> 19) & 3] = extended_to_double(v);
}

// Recompiled code entry points (generated): run from c.m_IP until it leaves
// recompiled code or the lockstep run ends.
bool has_code(uint32_t addr);   // generated
void run(Env &e);               // generated: dispatches into the chunk holding c.m_IP
uint64_t native_instructions(); // generated: instructions recompiled (all of them: there is no fallback)

} // namespace gen
