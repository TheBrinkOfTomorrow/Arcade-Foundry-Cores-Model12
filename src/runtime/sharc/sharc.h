// license:BSD-3-Clause
// copyright-holders:Ville Linde
//
// The Model 2B geometry processor: an Analog Devices ADSP-21062 "SHARC". Its
// semantics are MAME's (src/devices/cpu/sharc at
// dddd73680656e355bb2b5beecab1167c9f07bf81, BSD-3-Clause; notice above kept
// as the licence requires), transplanted by scripts/transplant_sharc.py into
// mame/*.inc around this class, which replaces MAME's device framework:
// internal memory is two arrays, the external port is SharcBus, and there are
// no timers, DMA after the host boot, or interrupts the board raises (Model
// 2B connects none). The program the i960 uploads is statically recompiled by
// tools/m2sharcrecomp, whose code calls these handlers with each
// instruction's opcode as a constant; nothing here runs an instruction from
// memory. See THIRD_PARTY.md.
#pragma once

#include <algorithm>
#include <cmath>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace rt::sharc {

// MAME's emu_fatalerror. The handlers' printf formats are kept as the
// message, followed by the arguments in hex.
struct SharcFatal : std::runtime_error {
    template <typename... A> explicit SharcFatal(const char *fmt, A... a) : std::runtime_error(message(fmt, a...)) {}

private:
    static std::string arg(const std::string &v) { return v; }
    static std::string arg(const char *v) { return v; }
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

// MAME's lib/util/coretmpl.h helpers the handlers use.
using offs_t = uint32_t;
using u32 = uint32_t;
namespace util {
template <typename T, typename U> constexpr T make_bitmask(U n) {
    return T((n < (8 * sizeof(T)) ? (std::make_unsigned_t<T>(1) << n) : std::make_unsigned_t<T>(0)) - 1);
}
template <typename T, typename U> constexpr std::make_signed_t<T> sext(T value, U width) noexcept {
    return std::make_signed_t<T>(value << (8 * sizeof(value) - width)) >> (8 * sizeof(value) - width);
}
template <typename T> T *little_endian_cast(uint32_t *p) { return reinterpret_cast<T *>(p); }
template <typename T> T *little_endian_cast(const uint32_t *p) { return reinterpret_cast<T *>(p); }
} // namespace util
template <typename T, typename U> constexpr T BIT(T x, U n) noexcept { return (x >> n) & T(1); }
template <typename T, typename U, typename V> constexpr T BIT(T x, U n, V w) { return (x >> n) & util::make_bitmask<T>(w); }
// MAME's osd/eminline.h
constexpr int64_t mul_32x32(int32_t a, int32_t b) { return int64_t(a) * int64_t(b); }
constexpr uint64_t mulu_32x32(uint32_t a, uint32_t b) { return uint64_t(a) * uint64_t(b); }
constexpr int32_t mul_32x32_shift(int32_t a, int32_t b, uint8_t shift) { return int32_t((int64_t(a) * int64_t(b)) >> shift); }
// int32_t(float) as x86-64 converts (MAME's reference build): values out of
// range, and NaN, give INT32_MIN (ARM would saturate).
inline int32_t x86_f2i(float f) {
    if (!(f >= -2147483648.0f && f < 2147483648.0f)) return INT32_MIN;
    return int32_t(f);
}
#define ACCESSING_BITS_0_15 ((mem_mask & 0x000000000000ffffULL) != 0)

static_assert(std::endian::native == std::endian::little, "the SHARC's short-word view assumes a little-endian host");

// The SHARC's external port (Model 2B: copro_sharc_map).
struct SharcBus {
    virtual uint32_t ext_r(uint32_t address) = 0;
    virtual void ext_w(uint32_t address, uint32_t data) = 0;
};

class Sharc;
using adsp21062_device = Sharc;

class Sharc {
public:
    Sharc();
    Sharc(const Sharc &) = delete;
    Sharc &operator=(const Sharc &) = delete;

    // MAME device_reset in BOOT_MODE_HOST: internal memory cleared, the boot
    // DMA (channel 6) set up for the host, the PC at the reset vector.
    void reset();
    // The host boot: the i960 writes the program 16 bits at a time while the
    // SHARC is held (MAME model2b copro_fifo_w).
    void external_dma_write(offs_t address, uint64_t data);
    // MAME external_iop_write: the i960's writes to the IOP registers (0x008c0000).
    void external_iop_write(offs_t address, uint32_t data);
    // MAME set_flag_input: the board's FIFO status lines.
    void set_flag_input(int flag_num, int state);

    SharcBus *bus = nullptr;
    uint64_t count = 0; // instructions executed
    // Held in reset by the board (before copro_boot): Last Bronx starts DMA
    // channel 6 from the i960 there and uploads through EPB0; in MAME that
    // DMA never runs (its state after the boot shows it never scheduled).
    bool held = true;

    // From MAME's sharc.h, for the transplanted handlers.
    enum sharc_boot_mode { BOOT_MODE_EPROM, BOOT_MODE_HOST, BOOT_MODE_LINK, BOOT_MODE_NOBOOT };

#include "runtime/sharc/mame/decls.inc"

    // Ours: what the device framework did.
    struct ProgramSpace {
        Sharc *s;
        inline uint64_t read_qword(uint32_t address);
        inline void write_qword(uint32_t address, uint64_t data, uint64_t mem_mask = ~uint64_t(0));
    };
    struct DataSpace {
        Sharc *s;
        inline uint32_t read_dword(uint32_t address);
        inline void write_dword(uint32_t address, uint32_t data);
    };
    void flag_out(unsigned, int) {} // Model 2B connects no output flag
    static std::string disassemble_one(uint32_t, uint64_t opcode) { // no disassembler here
        char b[24];
        std::snprintf(b, sizeof b, "%012llx", (unsigned long long)opcode);
        return b;
    }

    // The recompiled code's view of one step of MAME's execute_run.
    inline void loop_check();

    std::unique_ptr<sharc_internal_state> m_heap_core;
    sharc_internal_state *m_core = nullptr;
    sharc_boot_mode m_boot_mode = BOOT_MODE_HOST;
    ProgramSpace m_program{this};
    DataSpace m_data{this};
    uint32_t m_blocks[2][0x8000];
    bool m_dm_short_se = false; // MAME's m_dm_short_view: short reads sign-extend (MODE1 SSE)
};

#include "runtime/sharc/mame/state.inc"

} // namespace rt::sharc
