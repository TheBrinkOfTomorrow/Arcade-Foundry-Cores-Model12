// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
//
// The Model 1 TGP (Fujitsu MB86233) and the V60's side of it: MAME's
// src/mame/sega/model1_m.cpp at dddd73680656e355bb2b5beecab1167c9f07bf81
// (BSD-3-Clause; notice above kept as the licence requires). The
// instruction semantics are rt::Tgp's (MAME's mb86233, src/runtime/tgp.h);
// this is the board around it, which differs from Model 2's:
//   * the FIFOs are in data space (0x100 reads the input FIFO, 0x400 writes
//     the output FIFO), not in the register file;
//   * I/O space has four copro RAM address ports and data ports, the table
//     ports (atan and 1/x computed differently) and a window onto the data
//     ROM (0x8000-0xffff, base set at 0x2e);
//   * the program is a 2,048-word mask ROM (315-5573), not uploaded.
// The generated TGP code (m2tgprecomp --model1) calls m1::Tgp, whose
// members below hide rt::Tgp's of the same name. See THIRD_PARTY.md.
#pragma once

#include "model1/replay_bus.h"
#include "runtime/tgp.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <utility>
#include <vector>

namespace m1 {

struct Tgp : rt::Tgp {
    // Board state (model1_state's copro_*), MAME's names without m_.
    uint32_t copro_ram_adr[4]{}, copro_data_base = 0;
    uint32_t *copro_ram = nullptr;      // 0x2000 words, shared with the V60 side
    const uint32_t *copro_data = nullptr; // data ROM (copro_data), copro_data_words long
    uint32_t copro_data_words = 0;
    std::deque<uint32_t> *in = nullptr, *out = nullptr;
    // Timed: the TGP cycle of each output word, and of the last input pop
    // (the generated code runs whole blocks, so these come from the cycle
    // count)
    std::deque<uint64_t> *out_time = nullptr;
    uint64_t time_base = 0, cycle_base = 0, last_pop = 0;
    uint64_t now() const { return time_base + (count + extra_cycles - cycle_base); }

    // Data space: RAM at 0x000-0x0ff and 0x200-0x3ff, the FIFOs.
    uint32_t data_r(uint16_t ea) {
        if (ea < 0x100 || (ea >= 0x200 && ea < 0x400)) return data[ea];
        if (ea == 0x100) {
            if (in->empty()) {
                stall = true; // the instruction runs again when there is data
                return 0;
            }
            const uint32_t v = in->front();
            in->pop_front();
            last_pop = now();
            return v;
        }
        return 0;
    }
    void data_w(uint16_t ea, uint32_t v) {
        if (ea < 0x100 || (ea >= 0x200 && ea < 0x400)) data[ea] = v;
        else if (ea == 0x400) {
            out->push_back(v);
            if (out_time) out_time->push_back(now());
        }
    }
    uint32_t io_r(uint16_t ea);
    void io_w(uint16_t ea, uint32_t v);
    uint32_t prog_r(uint16_t ea) const { return ea < 0x800 ? prog[ea] : 0; }

    // The register file has nothing mapped but the LEDs (write only).
    template <uint32_t RR> uint32_t read_reg() {
        if constexpr ((RR & 0x3f) >= 0x20 && (RR & 0x3f) < 0x30) return 0;
        else return rt::Tgp::read_reg<RR>();
    }
    template <uint32_t RR> void write_reg(uint32_t v) {
        if constexpr ((RR & 0x3f) >= 0x20 && (RR & 0x3f) < 0x30) (void)v;
        else rt::Tgp::write_reg<RR>(v);
    }
    template <uint32_t R> void write_mem_internal_1(uint32_t v, bool bank) {
        uint16_t ea = ea_pre_1<R>();
        if (bank) ea += 0x200;
        data_w(ea, v);
        ea_post_1<R>();
    }
    template <uint32_t R> void write_mem_io_1(uint32_t v) {
        io_w(ea_pre_1<R>(), v);
        ea_post_1<R>();
    }

    // MAME's mb86233 takes a second cycle for these ALU ops' results
    // (alu_post_2: "assume each one takes 2 cycles"); the timed board counts them.
    uint64_t extra_cycles = 0;
    template <uint32_t ALU> void alu_post_2() {
        rt::Tgp::alu_post_2<ALU>();
        if constexpr (ALU == 0x05 || ALU == 0x06 || ALU == 0x07 || ALU == 0x0b || ALU == 0x0c || ALU == 0x10 ||
                      ALU == 0x11 || ALU == 0x13 || ALU == 0x14 || ALU == 0x08 || ALU == 0x09 || ALU == 0x0a ||
                      ALU == 0x0d)
            ++extra_cycles;
    }

    void board_reset(); // model1_state::copro_reset, TGP side
};

// Recompiled TGP program (m2tgprecomp --model1).
namespace tgpgen {
// Runs from t.pc until the TGP stalls on an empty input FIFO (t.pc at the
// stalled instruction) or `budget` instructions have completed.
void run(Tgp &t, uint64_t budget);
extern const uint32_t program_crc32;
extern const uint32_t program_words;
} // namespace tgpgen

// The coprocessor board as the V60 sees it (D00000-DFFFFF, in both memory
// and I/O space): the copro RAM address and data ports, the FIFOs and the
// input FIFO status. The V60's bus is 16 bits wide: a 32-bit access is two
// 16-bit ones, low half first (MAME's handlers are 16-bit). Untimed, the
// TGP runs when the V60 needs something from it, until it waits for input.
//
// Timed (set_clock): both run on one clock, as under MAME (V60 16 MHz at 8
// cycles an instruction; TGP 40 MHz / 3, one cycle an instruction and two
// for some ALU ops; ticks of 80 MHz, 5 a V60 cycle and 6 a TGP cycle). The TGP runs up to the
// start of the V60's timeslice before each access (MAME runs the V60 first
// in each), so copro RAM and FIFO reads see what it has written by then (its
// output words carry their time, as the generated code runs whole blocks); a
// FIFO read the TGP has not answered yet, or a write to a full input FIFO (16
// words), waits for it as MAME's FIFO makes the V60 wait (wait_out, wait_in),
// and the wait is counted (take_stall). The TGP halts when it fills the
// output FIFO (16 words) until the V60 reads.
class Copro : public DeviceModel {
public:
    // tgp_copro (0x2000 bytes), copro_tables (0x40000), copro_data (0x200000)
    Copro(const std::vector<uint8_t> &program, const std::vector<uint8_t> &tables,
          const std::vector<uint8_t> &data);
    Copro(const Copro &) = delete;
    Copro &operator=(const Copro &) = delete;

    void reset();
    static bool claims(uint32_t a) { return (a & 0xf00000) == 0xd00000; }
    uint32_t read(uint32_t a, unsigned size) override;
    void write(uint32_t a, uint32_t v, unsigned size) override;

    // Run the TGP until it waits on an empty input FIFO.
    void sync();
    static constexpr uint64_t kTicksPerV60Cycle = 5;
    // MAME's clocks in attoseconds: a tick (80 MHz) and a TGP cycle (40 MHz
    // over 3, rounded up to 13,333,334 Hz, so not quite 6 ticks). The TGP's
    // time is its cycle count: MAME adds whole cycles to it, halted or not.
    static constexpr int64_t kAttoPerTick = 12500000000, kAttoPerTgpCycle = 74999996250;
    static __int128 atto(uint64_t ticks) { return (__int128)ticks * kAttoPerTick; }
    static __int128 tgp_atto(uint64_t cycles) { return (__int128)cycles * kAttoPerTgpCycle; }
    uint64_t tgp_end(uint64_t t) const;
    static constexpr size_t kFifoDepth = 16;
    // MAME's timeslice: its UART clock's timer, every microsecond, ends one.
    // In a slice the V60 runs first, then the TGP, so the V60 sees what the
    // TGP did up to the slice's start, and the TGP sees the V60's writes of
    // the whole slice.
    static constexpr uint64_t kSliceTicks = 80;
    void set_slice(uint64_t ticks) { slice_ = ticks ? ticks : 1; }
    // A board timer's tick (MAME's irq0 timer) ends a slice too.
    void timer_tick(uint64_t t) { timer_sync_ = t; }
    // Another device's timers end slices too (Wing War's I/O board): the
    // last end at or before a time, both in attoseconds (0: none)
    void set_cuts(std::function<__int128(__int128)> cut) { cut_ = std::move(cut); }
    // The start of the V60's timeslice at time t (ticks), in attoseconds
    __int128 slice_start_atto(uint64_t t) const {
        const __int128 s = atto(slice_start(t));
        if (!cut_) return s;
        return std::max(s, cut_(atto(t)));
    }
    // Timed: now() is the V60's time in ticks.
    void set_clock(std::function<uint64_t()> now) { now_ = std::move(now); }
    // Ticks the V60 waited for the TGP since the last call.
    uint64_t take_stall() { return std::exchange(stall_, 0); }
    uint64_t wait_ticks() const override { return last_wait_; }
    uint64_t tgp_instructions() const { return tgp_.count; }
    size_t out_words() const { return out_.size(); }
    const uint32_t *ram() const { return ram_.data(); }

private:
    uint16_t read16(uint32_t a);
    void write16(uint32_t a, uint16_t v);
    void run_until(uint64_t t);       // timed: the TGP up to time t (or until it waits)
    void restart(uint64_t t);         // the V60 gave a halted TGP input or room at time t
    bool run_some(uint64_t n);        // n instructions or until it waits for input; false: it waits
    void wait_out(uint64_t from); // a pop from an empty output FIFO
    void wait_in(uint64_t from);  // a push to a full input FIFO
    static constexpr uint64_t kRetryTicks = 8 * kTicksPerV60Cycle; // a repeated V60 instruction
    // A slice starts on MAME's one-microsecond grid, where the V60's stall
    // last synced the machine (sync_), or at a board timer's tick
    uint64_t slice_start(uint64_t t) const {
        const uint64_t s = std::max({t - t % slice_, sync_ <= t ? sync_ : 0, timer_sync_ <= t ? timer_sync_ : 0});
        if (!cut_) return s;
        // a cut between two ticks: the TGP runs up to the tick before it
        return std::max(s, uint64_t(cut_(atto(t)) / kAttoPerTick));
    }
    // A halted CPU's time moves on in whole cycles: the V60's wait from
    // `from` to the TGP's cycle `cycle`
    static uint64_t v60_cycles(uint64_t cycle, uint64_t from) {
        const __int128 d = tgp_atto(cycle) - atto(from);
        return d > 0 ? uint64_t(d / (kAttoPerTick * kTicksPerV60Cycle)) * kTicksPerV60Cycle : 0;
    }
    // When the halted V60 starts again after the TGP's FIFO access at
    // `cycle`: the release aborts the TGP's slice after that instruction,
    // the sound CPU (10 MHz, before the halted V60 in MAME's list) runs up to
    // there, in whole cycles, and the V60 eats whole cycles up to the sound
    // CPU. The wait from `from`, in ticks.
    static uint64_t v60_resume(uint64_t cycle, uint64_t from) {
        uint64_t t = uint64_t(tgp_atto(cycle + 1) / kAttoPerTick);
        t -= t % kTicksPerSoundCycle;
        return t > from ? (t - from) / kTicksPerV60Cycle * kTicksPerV60Cycle : 0;
    }
    static constexpr uint64_t kTicksPerSoundCycle = 8;

    Tgp tgp_;
    std::vector<uint32_t> tables_, data_, ram_;
    std::deque<uint32_t> in_, out_;
    std::deque<uint64_t> out_time_;
    uint16_t v60_ram_adr_ = 0, v60_ram_latch_[2]{};
    uint32_t v60_fifo_r_ = 0, v60_fifo_w_ = 0;
    std::function<uint64_t()> now_;
    uint64_t tgp_time_ = 0, stall_ = 0; // tgp_time_ in TGP cycles
    bool halted_ = false; // waiting for input or room
    uint64_t slice_ = kSliceTicks, sync_ = 0, timer_sync_ = 0;
    uint64_t last_wait_ = 0;
    std::function<__int128(__int128)> cut_;
};

} // namespace m1
