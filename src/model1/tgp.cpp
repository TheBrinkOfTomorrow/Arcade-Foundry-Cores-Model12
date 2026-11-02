// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
//
// The Model 1 TGP's board: MAME's src/mame/sega/model1_m.cpp at
// dddd73680656e355bb2b5beecab1167c9f07bf81 (BSD-3-Clause; notice above kept
// as the licence requires). See tgp.h and THIRD_PARTY.md.

#include "model1/tgp.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace m1 {

namespace {
std::vector<uint32_t> words(const std::vector<uint8_t> &b) {
    std::vector<uint32_t> w(b.size() / 4);
    for (size_t i = 0; i < w.size(); i++)
        w[i] = b[i * 4] | b[i * 4 + 1] << 8 | b[i * 4 + 2] << 16 | uint32_t(b[i * 4 + 3]) << 24;
    return w;
}
} // namespace

void Tgp::board_reset() {
    std::fill(std::begin(copro_ram_adr), std::end(copro_ram_adr), 0);
    sincos_base = inv_base = isqrt_base = 0;
    std::fill(std::begin(atan_base), std::end(atan_base), 0);
}

// copro_io_map. Ports 0x00/0x08/0x10/0x18 are the four RAM address
// registers, 0x01/0x09/0x11/0x19 their data ports.
uint32_t Tgp::io_r(uint16_t ea) {
    if (ea >= 0x8000) { // copro_data_r
        uint32_t index = (copro_data_base & ~0x7fffu) | (ea - 0x8000u);
        index &= copro_data_words - 1;
        return copro_data[index];
    }
    if ((ea & ~0x18u) == 0x00) return copro_ram_adr[ea >> 3]; // copro_ramadr_r
    if ((ea & ~0x18u) == 0x01) {                             // copro_ramdata_r
        uint32_t &adr = copro_ram_adr[ea >> 3];
        const uint32_t val = copro_ram[adr & 0x1fff];
        adr += (adr & 0x40000) ? 4 : 1;
        return val;
    }
    switch (ea) {
    case 0x20: case 0x21: case 0x22: case 0x23: { // copro_sincos_r
        const uint32_t ang = sincos_base + (ea - 0x20u) * 0x4000;
        uint32_t index = ang & 0x3fff;
        if (ang & 0x4000) index = uint32_t(std::min(0x4000 - int(index), 0x3fff));
        uint32_t result = tables[index];
        if (ang & 0x8000) result ^= 0x80000000;
        return result;
    }
    case 0x24: case 0x25: case 0x26: case 0x27: { // copro_atan_r
        uint32_t idx = atan_base[3] & 0xffff;
        if (idx & 0xc000) idx = 0x3fff;
        uint32_t result = tables[idx | 0x4000];
        // MAME: "Correct for table bug, it seems that the hardware does
        // something equivalent somehow, or maybe there are boards with
        // updated opr roms"
        const uint16_t dt = uint16_t((result >> 16) + result);
        if (dt & 0x001) {
            if ((result & 0x00f) == 0x00e) result -= 0x00000001;
            else result -= 0x00010000;
        }
        if (dt & 0x010) {
            if ((result & 0x0f0) == 0x0e0) result -= 0x00000010;
            else result -= 0x00100000;
        }
        if (dt & 0x100) {
            if ((result & 0xf00) == 0xe00) result -= 0x00000100;
            else result -= 0x01000000;
        }
        const bool s0 = atan_base[0] & 0x80000000;
        const bool s1 = atan_base[1] & 0x80000000;
        const bool s2 = atan_base[2] & 0x80000000;
        if (s0 ^ s1 ^ s2) result >>= 16;
        if (s2) result += 0x4000;
        if ((s0 && !s2) || (s1 && s2)) result += 0x8000;
        return result & 0xffff;
    }
    case 0x28: case 0x29: { // copro_inv_r
        const uint32_t offset = ea - 0x28u;
        const uint32_t index = ((inv_base >> 9) & 0x3ffe) | (offset & 1);
        uint32_t result = tables[index | 0x8000];
        const uint8_t bexp = (inv_base >> 23) & 0xff;
        const uint8_t exp = uint8_t((result >> 23) + (0x7f - bexp));
        result = (result & 0x807fffff) | (uint32_t(exp) << 23);
        if (inv_base & 0x80000000) result ^= 0x80000000;
        return result;
    }
    case 0x2a: case 0x2b: { // copro_isqrt_r
        const uint32_t offset = ea - 0x2au;
        const uint32_t index = 0x2000 ^ (((isqrt_base >> 10) & 0x3ffe) | (offset & 1));
        uint32_t result = tables[index | 0xc000];
        const uint8_t bexp = (isqrt_base >> 24) & 0x7f;
        const uint8_t exp = uint8_t((result >> 23) + (0x3f - bexp));
        result = (result & 0x807fffff) | (uint32_t(exp) << 23);
        if (!(offset & 1)) result &= 0x7fffffff;
        return result;
    }
    default: return 0; // unmapped
    }
}

void Tgp::io_w(uint16_t ea, uint32_t v) {
    if (ea >= 0x8000) return; // the data ROM
    if ((ea & ~0x18u) == 0x00) { copro_ram_adr[ea >> 3] = v; return; } // copro_ramadr_w
    if ((ea & ~0x18u) == 0x01) {                                       // copro_ramdata_w
        uint32_t &adr = copro_ram_adr[ea >> 3];
        copro_ram[adr & 0x1fff] = v;
        adr += (adr & 0x40000) ? 4 : 1;
        return;
    }
    switch (ea) {
    case 0x20: case 0x21: case 0x22: case 0x23: sincos_base = v; break;
    case 0x24: case 0x25: case 0x26: case 0x27: atan_base[ea - 0x24] = v; break;
    case 0x28: case 0x29: inv_base = v; break;
    case 0x2a: case 0x2b: isqrt_base = v; break;
    case 0x2e: copro_data_base = v; break;
    default: break; // unmapped
    }
}

Copro::Copro(const std::vector<uint8_t> &program, const std::vector<uint8_t> &tables,
             const std::vector<uint8_t> &data)
    : tables_(words(tables)), data_(words(data)), ram_(0x2000, 0) {
    const std::vector<uint32_t> prog = words(program);
    if (prog.size() != 0x800 || tables_.size() != 0x10000 || data_.empty() || (data_.size() & (data_.size() - 1)))
        throw std::runtime_error("model1 copro: unexpected ROM sizes");
    std::copy(prog.begin(), prog.end(), tgp_.prog);
    tgp_.tables = tables_.data();
    tgp_.copro_ram = ram_.data();
    tgp_.copro_data = data_.data();
    tgp_.copro_data_words = uint32_t(data_.size());
    tgp_.in = &in_;
    tgp_.out = &out_;
    tgp_.out_time = &out_time_;
    reset();
}

void Copro::reset() {
    tgp_.reset();
    tgp_.board_reset();
    std::fill(ram_.begin(), ram_.end(), 0);
    std::fill(std::begin(tgp_.data), std::end(tgp_.data), 0);
    in_.clear();
    out_.clear();
    out_time_.clear();
    v60_ram_adr_ = 0;
    v60_ram_latch_[0] = v60_ram_latch_[1] = 0;
    v60_fifo_r_ = v60_fifo_w_ = 0;
    sync_ = timer_sync_ = 0;
}

void Copro::sync() {
    if (now_) run_until(slice_start(now_()));
    else tgpgen::run(tgp_, UINT64_MAX);
}

bool Copro::run_some(uint64_t n) {
    const uint64_t before = tgp_.count, cycles = tgp_.count + tgp_.extra_cycles;
    tgp_.time_base = tgp_time_, tgp_.cycle_base = cycles;
    tgpgen::run(tgp_, before + n);
    const uint64_t done = tgp_.count - before;
    tgp_time_ += tgp_.count + tgp_.extra_cycles - cycles;
    return done >= n; // the generated code returns early only when it waits for input
}

// The TGP's cycle at time t, as MAME's scheduler counts the cycles that fit
// (its divide drops the low bits of the time and of the cycle's length)
uint64_t Copro::tgp_end(uint64_t t) const {
    const __int128 d = atto(t) - tgp_atto(tgp_time_);
    return d > 0 ? tgp_time_ + uint64_t(d >> 6) / uint64_t(kAttoPerTgpCycle >> 6) : tgp_time_;
}

// The TGP's time follows the V60's: it idles (its time catches up) while it
// waits for input or for room in the output FIFO. Halted that way, it starts
// again when the V60 gives it input (room): releasing its halt line syncs
// the machine at the V60's access (restart).
//
// MAME gives a CPU the whole cycles that fit before the slice's end and runs
// instructions while some are left, so the TGP's last instruction in a
// slice starts at least a cycle before the end (and a two-cycle one may end
// a cycle past it). The runs here never take more cycles than are left: at
// most two an instruction.
void Copro::run_until(uint64_t t) {
    const uint64_t end = tgp_end(t);
    while (tgp_time_ < end) {
        const uint64_t left = end - tgp_time_;
        if (out_.size() >= kFifoDepth || !run_some(std::min<uint64_t>(left > 1 ? left / 2 : 1, 64))) {
            halted_ = true;
            break;
        }
    }
    if (halted_) tgp_time_ = std::max(tgp_time_, end);
}

void Copro::restart(uint64_t t) {
    if (!halted_) return;
    halted_ = false;
    tgp_time_ = tgp_end(t);
}

// MAME's generic FIFO: a pop from an empty FIFO or a push to a full one
// syncs the machine (the TGP runs up to the V60's time). A pop the TGP has
// answered by then is retried (MAME's stall: the V60 repeats the
// instruction, one instruction's time); a push it has made room for has
// already gone in (MAME keeps the word aside and the V60 runs on). Still
// empty (full): the V60 halts until the TGP pushes (pops). Releasing the
// halt line syncs the machine again, so the V60 starts at that moment (a
// halted CPU runs last and only up to it), and repeats the pop. `from` is
// the V60's time at the access.
void Copro::wait_out(uint64_t from) {
    run_until(from);
    if (!out_.empty() && tgp_atto(out_time_.front() + 1) <= atto(from)) {
        last_wait_ = kRetryTicks;
        sync_ = from + kRetryTicks;
    } else {
        while (out_.empty())
            if (!run_some(1)) throw std::runtime_error("model1 copro: the V60 and the TGP wait for each other");
        last_wait_ = v60_resume(out_time_.front(), from + kRetryTicks) + kRetryTicks; // halted after the failed pop
    }
    stall_ += last_wait_;
}

void Copro::wait_in(uint64_t from) {
    run_until(from);
    if (in_.size() >= kFifoDepth) {
        while (in_.size() >= kFifoDepth)
            if (out_.size() >= kFifoDepth || !run_some(1))
                throw std::runtime_error("model1 copro: the V60 and the TGP wait for each other");
        last_wait_ = v60_cycles(tgp_.last_pop, from);
    }
    stall_ += last_wait_;
}

uint16_t Copro::read16(uint32_t a) {
    if ((a & 0xfe0000) == 0xd00000) return v60_ram_adr_; // v60_copro_ram_adr_r
    if ((a & 0xfe0000) == 0xd20000) {                    // v60_copro_ram_r
        sync();
        if (!(a & 2)) return uint16_t(ram_[v60_ram_adr_ & 0x1fff]);
        const uint16_t r = uint16_t(ram_[v60_ram_adr_ & 0x1fff] >> 16);
        if (v60_ram_adr_ & 0x8000) v60_ram_adr_++;
        return r;
    }
    if ((a & 0xfe0000) == 0xd80000) { // v60_copro_fifo_r
        if (!(a & 2)) {
            if (now_) {
                const uint64_t t = now_();
                last_wait_ = 0;
                // the TGP's words up to the start of the V60's slice
                run_until(slice_start(t));
                if (out_.empty() || tgp_atto(out_time_.front() + 1) > atto(slice_start(t))) wait_out(t);
            } else if (out_.empty()) {
                sync();
            }
            if (out_.empty()) throw std::runtime_error("model1 copro: the V60 reads the TGP's FIFO, which waits for input");
            v60_fifo_r_ = out_.front();
            out_.pop_front();
            if (!out_time_.empty()) out_time_.pop_front();
            if (now_) restart(now_() + last_wait_ + kRetryTicks); // MAME's V60 is at its instruction's end
            return uint16_t(v60_fifo_r_);
        }
        return uint16_t(v60_fifo_r_ >> 16);
    }
    if ((a & 0xfe0000) == 0xdc0000) return 0xffff; // fifoin_status_r
    throw std::runtime_error("model1 copro: read of an unmapped address");
}

void Copro::write16(uint32_t a, uint16_t v) {
    if ((a & 0xfe0000) == 0xd00000) { v60_ram_adr_ = v; return; }
    if ((a & 0xfe0000) == 0xd20000) { // v60_copro_ram_w
        const unsigned offset = (a >> 1) & 1;
        v60_ram_latch_[offset] = v;
        if (offset) {
            sync(); // the TGP's writes so far land first
            ram_[v60_ram_adr_ & 0x1fff] = v60_ram_latch_[0] | uint32_t(v60_ram_latch_[1]) << 16;
            if (v60_ram_adr_ & 0x8000) v60_ram_adr_++;
        }
        return;
    }
    if ((a & 0xfe0000) == 0xd80000) { // v60_copro_fifo_w
        if (a & 2) {
            v60_fifo_w_ = (v60_fifo_w_ & 0x0000ffff) | uint32_t(v) << 16;
            if (now_) {
                const uint64_t t = now_();
                last_wait_ = 0;
                run_until(slice_start(t));
                if (in_.size() >= kFifoDepth) wait_in(t);
            }
            in_.push_back(v60_fifo_w_);
            if (now_) restart(now_() + last_wait_ + kRetryTicks); // MAME's V60 is at its instruction's end
        } else {
            v60_fifo_w_ = (v60_fifo_w_ & 0xffff0000) | v;
        }
        return;
    }
    if ((a & 0xfe0000) == 0xdc0000) return; // read only
    throw std::runtime_error("model1 copro: write to an unmapped address");
}

uint32_t Copro::read(uint32_t a, unsigned size) {
    if (size == 4) {
        const uint16_t lo = read16(a);
        return lo | uint32_t(read16(a + 2)) << 16;
    }
    const uint16_t w = read16(a & ~1u);
    return size == 1 ? (a & 1 ? w >> 8 : w & 0xff) : w;
}

void Copro::write(uint32_t a, uint32_t v, unsigned size) {
    if (size == 4) {
        write16(a, uint16_t(v));
        write16(a + 2, uint16_t(v >> 16));
        return;
    }
    if (size == 1) throw std::runtime_error("model1 copro: byte write");
    write16(a, uint16_t(v));
}

} // namespace m1
