// The model1io2 I/O board (io_board2.h): MAME's model1io2.cpp, 315_5338a.cpp,
// msm6253.cpp, eepromser.cpp and z80ctc.cpp (BSD-3-Clause; see
// THIRD_PARTY.md), with the device framework replaced by plain members and
// MAME's times by T-states.
#include "model1/io_board2.h"

#include <algorithm>
#include <stdexcept>

namespace m1 {

IoBoard2::IoBoard2(const std::vector<uint8_t> &bios, const std::vector<uint8_t> &e) {
    if (bios.size() < 0x8000) throw std::runtime_error("model1io2: no BIOS");
    std::copy_n(bios.begin(), 0x8000, z_.mem);
    z_.io = this;
    eeprom.fill(0xff); // MAME's empty EEPROM: every word 0xffff
    if (e.size() >= eeprom.size()) std::copy_n(e.begin(), eeprom.size(), eeprom.begin());
    controls.adc = {0x80, 0x80, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff}; // MAME's wingwar ports at rest
}

void IoBoard2::run_to(__int128 t) {
    if (t <= 0) return;
    const uint64_t until = uint64_t(t / kAttoPerCycle);
    if (until > z_.cycles) io2::run(z_, until);
}

void IoBoard2::sync() {
    if (now_ && slice_start_) run_to(slice_start_(now_()));
}

__int128 IoBoard2::latest_cut(__int128 t) {
    run_to(t - t % (80 * (__int128)kAttoPerTick)); // the Z80's timers set up by the microsecond before
    const uint64_t c = uint64_t(t / kAttoPerCycle);
    uint64_t best = 0;
    for (const Channel &ch : ctc_) {
        if (!ch.period || c < ch.start + ch.period) continue;
        const uint64_t f = ch.start + (c - ch.start) / ch.period * ch.period; // the last tick
        best = std::max(best, f + 1 <= c ? f + 1 : f); // its zero-count pulse ends a T-state later
    }
    if (wdt_due_ && wdt_due_ <= c) best = std::max(best, wdt_due_);
    __int128 r = at(best);
    if (ee_cut_ && ee_cut_ <= t) r = std::max(r, ee_cut_);
    return r;
}

// The V60's side: the low byte of each word
uint32_t IoBoard2::read(uint32_t a, unsigned size) {
    sync();
    const uint32_t i = (a & 0xfff) >> 1;
    if (on_right) {
        on_right('R', i & 0x7ff, ram_[i & 0x7ff], now_());
        if (size == 4) on_right('R', (i + 1) & 0x7ff, ram_[(i + 1) & 0x7ff], now_());
    }
    if (size == 4) return ram_[i & 0x7ff] | uint32_t(ram_[(i + 1) & 0x7ff]) << 16;
    if (size == 1 && (a & 1)) return 0;
    return ram_[i & 0x7ff];
}

void IoBoard2::write(uint32_t a, uint32_t v, unsigned size) {
    sync();
    const uint32_t i = (a & 0xfff) >> 1;
    if (size == 4) {
        ram_[i & 0x7ff] = uint8_t(v);
        ram_[(i + 1) & 0x7ff] = uint8_t(v >> 16);
        if (on_right) on_right('W', i & 0x7ff, uint8_t(v), now_()), on_right('W', (i + 1) & 0x7ff, uint8_t(v >> 16), now_());
    } else if (size == 2 || !(a & 1)) {
        ram_[i & 0x7ff] = uint8_t(v);
        if (on_right) on_right('W', i & 0x7ff, uint8_t(v), now_());
    }
}

// The Z80's memory map (model1io2_device::mem_map)
uint8_t IoBoard2::mem_r(uint16_t a, uint64_t cycles) {
    if (a < 0x8010) return io_r(a & 0x0f, cycles);
    if (a == 0x8040) // the board: buttons 0-3 and the jumpers off, the EEPROM's DO, bit 7 unused
        return uint8_t(0xbf | ee_do(cycles) << 6);
    if (a == 0x8080) return 0xff; // DSW1
    if (a >= 0x8100 && a < 0x8110) return fpga_count_ < 0x1400 ? 0x80 : 0xff;
    if ((a & 0xfff8) == 0x8200) { // MSM6253 d0_r: the latch's MSB, the rest unmapped
        const uint8_t b = adc_shift_ >> 7;
        adc_shift_ = uint8_t(adc_shift_ << 1);
        return uint8_t(0xfe | b);
    }
    return 0xff;
}

void IoBoard2::mem_w(uint16_t a, uint8_t v, uint64_t cycles) {
    if (a < 0x8010) return io_w(a & 0x0f, v, cycles);
    if (a >= 0x8100 && a < 0x8110) { ++fpga_count_; return; }
    if ((a & 0xfff8) == 0x8200) { // address_w: the channel's value into the shift register
        const unsigned ch = (a & 3) + (secondary_ ? 4 : 0);
        adc_shift_ = controls.adc[ch];
    }
}

// 315-5338A
uint8_t IoBoard2::io_r(unsigned offset, uint64_t cycles) {
    switch (offset) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6:
        if (port_config_ >> offset & 1) {
            switch (offset) {
            case 0: return controls.in0;
            case 1: return controls.in1;
            case 2: return controls.in2;
            default: return 0xff; // port E: the drive board (none)
            }
        }
        return port_value_[offset];
    case 0x08: return port_config_;
    case 0x0a: return serial_;
    case 0x0b: return cmd_;
    case 0x0c: // the dual-port RAM's left side
        if (on_left) on_left('R', address_ & 0x7ff, ram_[address_ & 0x7ff], cycles);
        return ram_[address_ & 0x7ff];
    case 0x0d: return 0x08;
    default: return 0xff;
    }
}

void IoBoard2::io_w(unsigned offset, uint8_t v, uint64_t cycles) {
    switch (offset) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6:
        port_value_[offset] = v;
        port_out(offset, v, cycles);
        break;
    case 0x08: {
        const uint8_t changed = v ^ port_config_;
        for (unsigned i = 0; i < 7; i++)
            if ((changed >> i & 1) && !(v >> i & 1)) port_out(i, port_value_[i], cycles);
        port_config_ = v;
        break;
    }
    case 0x09:
        cmd_ = v;
        if (v == 0x00) address_ = uint16_t((address_ & 0xff00) | serial_);
        else if (v == 0x01) address_ = uint16_t((address_ & 0x00ff) | serial_ << 8);
        else if (v == 0x07 || (v >= 0x70 && v <= 0x77)) {
            const unsigned i = v == 0x07 ? address_ & 0x7ffu : v & 7u;
            ram_[i] = serial_;
            if (on_left) on_left('W', i, serial_, cycles);
        }
        break;
    case 0x0a: serial_ = v; break;
    default: break;
    }
}

// The ports' outputs: D the lamps, F the EEPROM's lines (and the LCD's), G
// the watchdog, the control panel switch and the comm LED
void IoBoard2::port_out(unsigned port, uint8_t v, uint64_t cycles) {
    if (port == 3) outputs = v;
    else if (port == 5) { // io_pf_w: clk, di, cs in that order
        ee_clk(v >> 5 & 1, cycles);
        ee_di_ = v >> 6 & 1;
        ee_cs(v >> 4 & 1, cycles);
    } else if (port == 6) {
        secondary_ = v >> 6 & 1;
    }
}

// 93C46
void IoBoard2::ee_store(unsigned a, uint16_t v) {
    eeprom[2 * a] = uint8_t(v >> 8);
    eeprom[2 * a + 1] = uint8_t(v);
    eeprom_dirty = true;
}

int IoBoard2::ee_do(uint64_t cycles) const {
    if (ee_state_ == Ee::StartBit) return ee_ready(cycles) ? 1 : 0; // DO is READY/BUSY
    if (ee_state_ == Ee::Reading) return ee_shift_ >> 31 & 1;
    return 1; // tri-state, read high
}

void IoBoard2::ee_set_state(Ee s, uint64_t cycles) {
    ee_state_ = s;
    // the write cycle's end, a timer in MAME (it ends a timeslice)
    if (s == Ee::Completion && !ee_ready(cycles)) ee_cut_ = ee_completion_;
}

void IoBoard2::ee_cs(int state, uint64_t cycles) {
    if (state == ee_cs_) return;
    ee_cs_ = state;
    if (state) {
        ee_cs_rise_ = at(cycles);
        if (ee_state_ == Ee::Reset) ee_set_state(Ee::StartBit, cycles);
        return;
    }
    switch (ee_state_) { // CS falling
    case Ee::StartBit: case Ee::Command: case Ee::Reading: case Ee::WaitData: case Ee::Completion:
        ee_set_state(Ee::Reset, cycles);
        break;
    default: break;
    }
}

void IoBoard2::ee_clk(int state, uint64_t cycles) {
    if (state == ee_clk_) return;
    ee_clk_ = state;
    if (state) ee_event_clk_rise(cycles);
}

void IoBoard2::ee_event_clk_rise(uint64_t cycles) {
    switch (ee_state_) {
    case Ee::StartBit:
        if (ee_di_ && ee_ready(cycles) && at(cycles) > ee_cs_rise_) {
            ee_accum_ = ee_bits_ = 0;
            ee_set_state(Ee::Command, cycles);
        }
        break;
    case Ee::Command:
        ee_accum_ = (ee_accum_ << 1) | unsigned(ee_di_);
        if (++ee_bits_ == 2 + 6) { // execute_command (6 address bits)
            ee_address_ = ee_accum_ & 0x3f;
            ee_bits_ = 0;
            switch (ee_accum_ >> 6) {
            case 0:
                switch (ee_address_ >> 4) {
                case 0: ee_command_ = 4; break; // lock
                case 1: ee_command_ = 6; break; // write all
                case 2: ee_command_ = 7; break; // erase all
                default: ee_command_ = 5; break; // unlock
                }
                ee_address_ = 0;
                break;
            case 1: ee_command_ = 2; break;
            case 2: ee_command_ = 1; break;
            default: ee_command_ = 3; break;
            }
            switch (ee_command_) {
            case 1: ee_shift_ = 0; ee_set_state(Ee::Reading, cycles); break;
            case 2: case 6: ee_shift_ = 0; ee_set_state(Ee::WaitData, cycles); break;
            case 3:
                if (ee_locked_) { ee_set_state(Ee::Reset, cycles); break; }
                ee_store(ee_address_, 0xffff);
                ee_completion_ = at(cycles) + (__int128)1000000 * 1000000000; // ERASE_TIME 1000 us
                ee_set_state(Ee::Completion, cycles);
                break;
            case 4: ee_locked_ = true; ee_set_state(Ee::Reset, cycles); break;
            case 5: ee_locked_ = false; ee_set_state(Ee::Reset, cycles); break;
            case 7:
                if (ee_locked_) { ee_set_state(Ee::Reset, cycles); break; }
                for (unsigned i = 0; i < 64; i++) ee_store(i, 0xffff);
                ee_completion_ = at(cycles) + (__int128)8000 * 1000000000000; // ERASE_ALL_TIME 8000 us
                ee_set_state(Ee::Completion, cycles);
                break;
            }
        }
        break;
    case Ee::Reading: {
        const unsigned bit = ee_bits_++;
        if (bit % 16 == 0 && bit == 0) // no streaming: one word, then 1s
            ee_shift_ = uint32_t(ee_word((ee_address_ + ee_bits_ / 16) & 0x3f)) << 16;
        else
            ee_shift_ = (ee_shift_ << 1) | 1;
        break;
    }
    case Ee::WaitData:
        ee_shift_ = (ee_shift_ << 1) | unsigned(ee_di_);
        if (++ee_bits_ == 16) {
            if (ee_locked_) { ee_set_state(Ee::Reset, cycles); break; }
            if (ee_command_ == 2) {
                ee_store(ee_address_, uint16_t(ee_shift_));
                ee_completion_ = at(cycles) + (__int128)1750 * 1000000000000; // WRITE_TIME 1750 us
            } else {
                for (unsigned i = 0; i < 64; i++) ee_store(i, uint16_t(ee_shift_));
                ee_completion_ = at(cycles) + (__int128)8000 * 1000000000000;
            }
            ee_set_state(Ee::Completion, cycles);
        }
        break;
    default: break;
    }
}

// The TMPZ84C015's I/O space: CTC 10-13, SIO 18-1B, PIO 1C-1F (DSW2, DSW3),
// watchdog F0/F1, interrupt priority F4
uint8_t IoBoard2::in(uint16_t port, uint64_t cycles) {
    ctc_to(cycles);
    switch (port & 0xff) {
    case 0x1c: case 0x1e: return 0xff; // DSW2, DSW3 (port A, B data)
    case 0xf0: return 0xfb;
    default: return 0xff;
    }
}

void IoBoard2::out(uint16_t port, uint8_t v, uint64_t cycles) {
    ctc_to(cycles);
    const unsigned p = port & 0xff;
    if (p >= 0x10 && p <= 0x13) return ctc_w(p & 3, v, cycles);
    if (p == 0xf1 && v == 0x4e) wdt_due_ = cycles + (0x10000u << 6); // watchdog_clear (WDTMR 0xfb: 2^22 T-states)
}

void IoBoard2::ctc_w(unsigned n, uint8_t v, uint64_t cycles) {
    Channel &ch = ctc_[n];
    auto start = [&] { // the timer from now: a tick every prescale x constant T-states
        const unsigned tc = ch.tconst ? ch.tconst : 0x100;
        ch.period = (ch.mode & 0x40) ? 0 : uint64_t((ch.mode & 0x20) ? 256 : 16) * tc; // counter mode: no clock here
        ch.start = cycles;
        ch.next = cycles + ch.period;
    };
    if (ch.mode & 0x04) { // the time constant
        ch.tconst = v;
        ch.mode &= uint8_t(~0x06);
        if ((ch.mode & 0x40) || !(ch.mode & 0x08)) start();
        else throw std::runtime_error("model1io2: CTC trigger mode");
        return;
    }
    if (!(v & 1)) { // the vector (channel 0)
        if (n == 0) ctc_vector_ = v & 0xf8;
        return;
    }
    if (v & 0x02) ch.period = 0; // reset: the timer stops
    ch.mode = v;
    if (!(v & 0x80)) ch.int_state &= uint8_t(~1); // interrupts off: the request goes
}

void IoBoard2::ctc_to(uint64_t cycles) {
    for (Channel &ch : ctc_) {
        if (!ch.period || ch.next > cycles) continue;
        if (ch.mode & 0x80) ch.int_state |= 1;
        ch.next += ((cycles - ch.next) / ch.period + 1) * ch.period;
    }
}

bool IoBoard2::irq(uint64_t cycles) {
    ctc_to(cycles);
    for (const Channel &ch : ctc_) {
        if (ch.int_state & 2) return false;
        if (ch.int_state & 1) return true;
    }
    return false;
}

uint8_t IoBoard2::irq_ack(uint64_t cycles) {
    ctc_to(cycles);
    for (unsigned n = 0; n < 4; n++)
        if (ctc_[n].int_state & 1) {
            ctc_[n].int_state = 2;
            return uint8_t(ctc_vector_ + n * 2);
        }
    return ctc_vector_;
}

void IoBoard2::reti(uint64_t cycles) {
    ctc_to(cycles);
    for (Channel &ch : ctc_)
        if (ch.int_state & 2) {
            ch.int_state &= uint8_t(~2);
            return;
        }
}

} // namespace m1
