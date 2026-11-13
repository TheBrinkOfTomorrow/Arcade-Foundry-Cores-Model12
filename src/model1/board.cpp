#include "af_debug.h"
#include "model1/board.h"

#include "v60/interp.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace m1 {

void NativeEnv::miss(v60::Core &c) {
    char b[96];
    std::snprintf(b, sizeof b, "V60: no recompiled code at %06x", c.m_reg[32] & 0xffffff);
    throw v60::Fatal(b);
}

Board::FallbackEnv::FallbackEnv() {
    if (const char *v = af_debug_env("M2_STRICT")) strict = *v && std::strcmp(v, "0") != 0;
}

// The same stop conditions as generated code: the limit before each
// instruction, and a pending interrupt the PSW lets in (run_native's loop
// test; only UPDPSW, LDPR, RETIU and RETIS can let one in).
void Board::FallbackEnv::miss(v60::Core &c) {
    if (strict) return NativeEnv::miss(c);
    const uint32_t pc = c.m_reg[32] & 0xffffff;
    if (seen.insert(pc).second)
        std::fprintf(stderr, "V60: fallback interpreter at %06x (a seed for the next recompile)\n", pc);
    do {
        if (count >= limit || (irq_pending && c.interrupts_enabled())) return;
        v60::Interp::step(c);
        ++count;
        ++interpreted;
    } while (!has_native(c, c.m_reg[32]));
}

Board::Board(Images img, const RomSet &set)
    : mem_(std::move(img.at("maincpu"))), copro_(img.at("tgp_copro"), img.at("copro_tables"), img.at("copro_data")),
      video_(mem_, img.at("polygons")), io_(img.at("eeprom")),
      sound_(img.at("m1audio"), img.at("mpcm1"), img.at("mpcm2"), set.sound_ips), cpu_(this) {
    const std::vector<uint8_t> &dsb_program = img.at("dsb_cpu");
    if (std::any_of(dsb_program.begin(), dsb_program.begin() + 0x100, [](uint8_t b) { return b != 0; }))
        dsb_ = std::make_unique<Dsb>(dsb_program, std::move(img.at("dsb_mpeg")));
    mem_.map(*this);
    copro_.set_clock([this] { return now(); });
    io_.set_clock([this] { return now(); });
    if (set.io_bios_b) io_.set_bios_b();
    if (set.io2) {
        // MAME's timeslices end at the I/O board's timers too: the TGP's and
        // the board's view of the V60's slice
        io2_ = std::make_unique<IoBoard2>(img.at("io2_bios"), std::vector<uint8_t>{});
        io2_->set_clock([this] { return now(); });
        copro_.set_cuts([this](__int128 t) { return io2_->latest_cut(t); });
        io2_->set_slices([this](uint64_t t) { return copro_.slice_start_atto(t); });
    }
    cpu_.reset();
}

void Board::state(rt::StateIO &s) {
    s.mark("M1B ");
    cpu_.state(s);
    s(env_.count), s(env_.limit), s(env_.irq_pending), s(env_.interpreted);
    s(frame_), s(next_vblank_), s(stall_ticks_), s(target_), s(sound_done_), s(irq_status_), s(irq_mask_), s(last_irq_);
    s(timer_period_), s(timer_val_), s(timer_due_), s(uart_phase_), s(uart_expect_mode_), s(uart_txen_), s(uart_full_);
    s(uart_due_), s(uart_shift_end_);
    mem_.state(s, *this);
    copro_.state(s);
    video_.state(s);
    s.mark("M1IO");
    bool io2 = io2_ != nullptr, dsb = dsb_ != nullptr;
    s(io2), s(dsb);
    if (io2 != (io2_ != nullptr) || dsb != (dsb_ != nullptr)) return s.fail("the save state's boards are another set's");
    if (io2_) io2_->state(s);
    else io_.state(s);
    comm_.state(s);
    sound_.state(s);
    if (dsb_) dsb_->state(s);
    s.mark("END ");
}

// The V60 waited for the TGP: its clock moves on, and the run stops on time.
void Board::copro_waited() {
    stall_ticks_ += copro_.take_stall();
    const uint64_t t = now();
    const uint64_t left = target_ > t ? (target_ - t + kTicksPerInstruction - 1) / kTicksPerInstruction : 1;
    env_.limit = std::min(env_.limit, env_.count + std::max<uint64_t>(left, 1));
}

// A level raised by an access is taken after the instruction (MAME
// synchronises the input line change)
void Board::irq_raise(int level) {
    irq_status_ |= uint8_t(1u << level);
    env_.irq_pending = true;
    env_.limit = std::min(env_.limit, env_.count + 1);
}

// The V60 takes the lowest pending level (irq_callback) whenever its PSW
// allows; while one is pending and masked by the PSW, the code runs in short
// slices so it is taken soon after the game enables interrupts again. A run
// stops at each timer tick.
void Board::run_to(uint64_t ticks) {
    while (now() < ticks) {
        fire_timers();
        target_ = std::min(ticks, std::min(std::min(timer_due_[0], timer_due_[1]), uart_due_));
        const uint64_t t = now();
        const uint64_t count = env_.count + (target_ > t ? (target_ - t + kTicksPerInstruction - 1) / kTicksPerInstruction : 1);
        if (irq_status_ && cpu_.interrupts_enabled()) {
            for (int i = 0; i < 8; i++)
                if (irq_status_ & (1u << i)) {
                    last_irq_ = i;
                    break;
                }
            cpu_.take_interrupt(last_irq_ + 0x40); // v60_try_irq: the level + 0x40
        }
        env_.limit = irq_status_ ? std::min(count, env_.count + 16) : count;
        env_.irq_pending = irq_status_ != 0;
        run_native(cpu_, env_);
    }
}

// irq0_timer_tick: each timer due raises level 0 and runs on from its tick.
// The UART's buffered byte starting raises TxRDY.
void Board::fire_timers() {
    for (int t = 0; t < 2; t++)
        while (timer_due_[t] <= now()) {
            if (!(irq_mask_ & 1)) irq_raise(0);
            copro_.timer_tick(timer_due_[t]);
            timer_due_[t] = timer_period_[t] ? timer_due_[t] + kTimerUnit * timer_period_[t] : kNever;
        }
    uart_update();
}

// MAME's i8251 at each bit time (transmit_clock): with the shift register
// empty it starts a buffered byte, and TxRDY's handler (sound_ready_w) runs,
// raising level 3 when unmasked; it runs again at every idle bit time.
void Board::uart_update() {
    while (uart_due_ <= now()) {
        const uint64_t t = uart_due_;
        if (uart_full_) {
            uart_full_ = false;
            uart_shift_end_ = t + 10 * kUartBitTicks;
        }
        uart_ready();
        uart_schedule(t);
    }
}

// The next bit time that does something, after `after`.
void Board::uart_schedule(uint64_t after) {
    const uint64_t tick = uart_next_bit(after);
    if (!uart_txen_) uart_due_ = kNever;
    else if (uart_full_) uart_due_ = std::max(uart_shift_end_, tick);
    else if (!(irq_mask_ & 8)) uart_due_ = std::max(uart_shift_end_, tick);
    else uart_due_ = kNever;
    if (uart_due_ < target_) { // the run stops there
        target_ = uart_due_;
        const uint64_t n = now();
        env_.limit = std::min(env_.limit, env_.count + (uart_due_ > n ? (uart_due_ - n + kTicksPerInstruction - 1) / kTicksPerInstruction : 1));
    }
}

uint64_t Board::uart_next_bit(uint64_t after) const {
    const uint64_t k = after >= uart_phase_ ? (after - uart_phase_) / kUartBitTicks + 1 : 0;
    return uart_phase_ + k * kUartBitTicks;
}

// The status register's TxRDY is the buffer's state (MAME's
// I8251_STATUS_TX_READY, set at reset): TxEN gates only the pin (uart_ready)
bool Board::uart_txrdy() {
    uart_update();
    return !uart_full_;
}

void Board::uart_ready() {
    if (uart_txen_ && !uart_full_ && !(irq_mask_ & 8)) irq_raise(3);
}

// data_w: with the shift register empty the byte starts at once (its first
// bit at the next bit time, the register empty again 10 bits on); otherwise
// it waits in the buffer (TxRDY low) for the register.
void Board::uart_data_w(uint8_t v) {
    uart_update();
    sound_to_now();
    sound_.send(&v, 1);
    const uint64_t n = now();
    if (!uart_full_ && n >= uart_shift_end_) {
        uart_shift_end_ = uart_next_bit(n) + 10 * kUartBitTicks;
        uart_ready();
    } else {
        uart_full_ = true;
    }
    uart_schedule(n);
}

// The mode after a reset (or at power on), then commands: TxEN is bit 0,
// internal reset bit 6.
void Board::uart_control_w(uint8_t v) {
    if (uart_expect_mode_) {
        // The mode write restarts the x16 divider: a bit time every 16th
        // falling edge of TxC (500 kHz, falling on each even microsecond) from here
        uart_expect_mode_ = false;
        const uint64_t edge = (now() / kTxcTicks + 1) * kTxcTicks;
        uart_phase_ = (edge + 15 * kTxcTicks) % kUartBitTicks;
        return;
    }
    if (v & 0x40) {
        uart_expect_mode_ = true;
        uart_txen_ = false;
        return;
    }
    uart_txen_ = v & 1;
    uart_ready();
    uart_schedule(now());
}

// timer_period_w: a write restarts the timer, a period of 0 stops it.
void Board::timer_period_w(int t, uint16_t period) {
    timer_period_[t] = period;
    timer_due_[t] = period ? now() + kTicksPerInstruction + kTimerUnit * period : kNever; // MAME: at the write's end
    if (timer_due_[t] < target_) { // the run stops at the new tick
        target_ = timer_due_[t];
        env_.limit = std::min(env_.limit, env_.count + (kTimerUnit * period + kTicksPerInstruction - 1) / kTicksPerInstruction);
    }
}

void Board::sound_to_now() {
    const double t = double(now()) / kTickHz;
    if (t > sound_done_) {
        sound_.advance(t - sound_done_);
        sound_done_ = t;
        if (dsb_) {
            for (const auto &[at, byte] : sound_.take_tx()) dsb_->receive(byte, at);
            dsb_->advance_to(t);
        }
    }
}

void Board::run_frame() {
    run_to(next_vblank_);
    comm_.vblank();
    if (!(irq_mask_ & 2)) irq_raise(1); // model1_interrupt, line 384
    video_.vblank(frame_);
    run_to(next_vblank_ + kUpdateTicks);
    video_.screen_update(frame_);
    sound_to_now();
    next_vblank_ += kFrameTicks;
    ++frame_;
}

uint8_t Board::comm_read(uint32_t a) {
    if ((a & 0xfff000) == 0xb00000) return comm_.share_r(a);
    if (a == 0xb01000) return comm_.cn_r();
    if (a == 0xb01002) return comm_.fg_r();
    return 0;
}

void Board::comm_write(uint32_t a, uint8_t v) {
    if ((a & 0xfff000) == 0xb00000) comm_.share_w(a, v);
    else if (a == 0xb01000) comm_.cn_w(v);
    else if (a == 0xb01002) comm_.fg_w(v);
}

uint32_t Board::dev_read(uint32_t a, unsigned size, unsigned space) {
    if (Copro::claims(a)) {
        const uint32_t v = copro_.read(a, size);
        copro_waited();
        return v;
    }
    if (space) return 0;
    if ((a & 0xfff000) == 0xc00000) {
        // dpram_r's wait state: a V60 cycle for each byte-lane access
        stall_ticks_ += (size == 4 ? 2 : 1) * Copro::kTicksPerV60Cycle;
        return io2_ ? io2_->read(a, size) : io_.read(a, size);
    }
    if ((a & ~3u) == 0x680000) return video_.read(a, size);
    if ((a & 0xffe000) == 0xb00000) {
        uint32_t v = 0;
        for (unsigned i = 0; i < size; i++) v |= uint32_t(comm_read(a + i)) << (8 * i);
        return v;
    }
    if (a == 0xe00002) return irq_mask_;
    if (a == 0xc40002) // i8251 status: DSR, TxEMPTY, TxRDY
        return 0x80u | (uart_txrdy() ? 1u : 0u) | (!uart_full_ && now() >= uart_shift_end_ ? 4u : 0u);
    if (a == 0xe0000c || a == 0xe0000e) { // timer_r
        const int t = (a >> 1) & 1;
        if (timer_period_[t]) {
            const uint64_t n = now();
            timer_val_[t] = uint16_t(timer_due_[t] > n ? (timer_due_[t] - n) / kTimerUnit : 0);
        }
        return timer_val_[t];
    }
    return 0;
}

void Board::dev_write(uint32_t a, uint32_t v, unsigned size, unsigned space) {
    if (Copro::claims(a)) {
        copro_.write(a, v, size);
        return copro_waited();
    }
    if (space) return;
    if ((a & 0xfff000) == 0xc00000) return io2_ ? io2_->write(a, v, size) : io_.write(a, v, size);
    if ((a & ~3u) == 0x680000) return video_.write(a, v, size);
    if ((a & 0xffe000) == 0xb00000) {
        for (unsigned i = 0; i < size; i++) comm_write(a + i, uint8_t(v >> (8 * i)));
        return;
    }
    if (a == 0xc40000) return uart_data_w(uint8_t(v)); // the sound board's UART
    if (a == 0xc40002) return uart_control_w(uint8_t(v));
    switch (a) {
    case 0xe00000: // irq_control_w
        if ((v & 0xff) == 0x10) irq_status_ = 0;
        else if ((v & 0xff) == 0x20) irq_status_ &= uint8_t(~(1u << last_irq_));
        break;
    case 0xe00002: // irq_mask_w (VR and swa never unmask the UART's level 3)
        uart_update();
        irq_mask_ = uint8_t(v);
        uart_ready();
        uart_schedule(now());
        break;
    case 0xe00004: mem_.bank_write(*this, v); break;
    case 0xe00008:
        timer_period_w(0, uint16_t(v));
        if (size == 4) timer_period_w(1, uint16_t(v >> 16));
        break;
    case 0xe0000a: timer_period_w(1, uint16_t(v)); break;
    default: break; // 720000-770000 sync registers, E00006 timer mode, E0000C/E counts (read only)
    }
}

} // namespace m1
