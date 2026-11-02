// The Digital Sound Board 2 (dsb2.h). The FIFO commands and the stream
// update are MAME's dsb2.cpp (license: BSD-3-Clause), the i8251's receive
// side as m1::Dsb's (after MAME's i8251), with the device framework replaced
// by plain members.
#include "runtime/dsb2.h"

#if M2_DSB2 // Top Skater's build only: dsb2gen, the recompiled program, is its own

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace snd {

Dsb2::Dsb2(const std::vector<uint8_t> &program, std::vector<uint8_t> mpeg) : program_(program), mpeg_rom_(std::move(mpeg)) {
    if (program_.size() < 0x20000) throw rt::Fatal("DSB2: no 68000 program (re-run the importer)");
    program_.resize(0x20000);
    cpu_.dsb2 = true;
    cpu_.ram.assign(0x20000, 0);
    cpu_.rom = program_.data();
    cpu_.dev = this;
    cpu_.reset();
    decoder_ = std::make_unique<m1::mpeg_audio>(mpeg_rom_.data(), m1::mpeg_audio::L2, false, 0);
    sched_.on_irq = [this](int level) {
        if (level == 2) timer_irq_ = false; // HOLD_LINE: cleared once taken
        update_irq();
    };
    sched_.at(kIps / 1000, [this] { tick_1khz(); });
    sched_.at(kIps / 50, [this] { update_stream(); });
}

void Dsb2::receive(uint8_t byte, double seconds) {
    sched_.at(uint64_t(std::llround(seconds * double(kIps))), [this, byte] { deliver(byte); });
}

void Dsb2::deliver(uint8_t byte) {
    if (!rx_enable_) return;
    if (rxrdy_) overrun_ = true;
    rx_data_ = byte, rxrdy_ = true;
    update_irq();
}

void Dsb2::update_irq() {
    cpu_.irq_line = timer_irq_ ? 2 : rx_enable_ && rxrdy_ ? 1 : 0;
    sched_.poke();
}

void Dsb2::tick_1khz() {
    timer_irq_ = true;
    update_irq();
    ++ms_;
    sched_.at(uint64_t(std::llround(double(ms_ + 1) * double(kIps) / 1000.0)), [this] { tick_1khz(); });
}

void Dsb2::advance_to(double seconds) {
    const uint64_t until = uint64_t(seconds * double(kIps));
    while (sched_.count < until) {
        sched_.end_count = until;
        if (!dsb2gen::has_code(cpu_.pc)) {
            char b[96];
            std::snprintf(b, sizeof b, "DSB2 68000: no recompiled code at %06x", cpu_.pc);
            throw rt::Fatal(b);
        }
        dsb2gen::run(env_);
    }
}

uint8_t Dsb2::read(uint32_t addr) {
    switch (addr) {
    case 0xc00001: // i8251 data
        rxrdy_ = false;
        update_irq();
        if (log_) std::fprintf(log_, "%.6f R c00001 %02x\n", now(), rx_data_);
        return rx_data_;
    case 0xc00003: // i8251 status: DSR, TxEMPTY, TxRDY; RxRDY; overrun
        return uint8_t(0x85 | (rxrdy_ ? 0x02 : 0) | (overrun_ ? 0x10 : 0));
    case 0xe80001: return 0x01; // MPEG status
    default: return 0;
    }
}

void Dsb2::write(uint32_t addr, uint16_t data) {
    const uint8_t v = uint8_t(data);
    switch (addr) {
    case 0xc00003: // i8251 mode / command
        if (expect_mode_) {
            expect_mode_ = false;
        } else if (v & 0x40) { // internal reset: a mode instruction follows
            expect_mode_ = configured_ = true, rx_enable_ = false;
        } else if (configured_) {
            rx_enable_ = v & 0x04;
            if (v & 0x10) overrun_ = false; // error reset
        }
        update_irq();
        break;
    case 0xd00001: // system control: bit 3 the MPEG ROM bank (A24, inverted; only with more than 16 MB)
        if (log_) std::fprintf(log_, "%.6f W d00001 %02x\n", now(), v);
        rom_bank_ = (~v >> 3) & 1;
        break;
    case 0xe00003:
        if (log_) std::fprintf(log_, "%.6f W e00003 %02x\n", now(), v);
        fifo_w(v);
        break;
    default: break; // the i8251's transmitter (no one listens, as in MAME), 0xe00001
    }
}

void Dsb2::fifo_w(uint8_t data) {
    switch (cmd_) {
    case Cmd::StartHi: start_ = (start_ & 0x00ffff) | uint32_t(data) << 16, cmd_ = Cmd::StartMd; return;
    case Cmd::StartMd: start_ = (start_ & 0xff00ff) | uint32_t(data) << 8, cmd_ = Cmd::StartLo; return;
    case Cmd::StartLo: start_ = (start_ & 0xffff00) | data, mp_start_ = start_, cmd_ = Cmd::Idle; return;
    case Cmd::EndHi: end_ = (end_ & 0x00ffff) | uint32_t(data) << 16, cmd_ = Cmd::EndMd; return;
    case Cmd::EndMd: end_ = (end_ & 0xff00ff) | uint32_t(data) << 8, cmd_ = Cmd::EndLo; return;
    case Cmd::EndLo: end_ = (end_ & 0xffff00) | data, mp_end_ = end_, cmd_ = Cmd::Idle; return;
    case Cmd::Idle: break;
    }
    // The opcodes' low bit is the player number, not part of the address.
    if ((data & 0xfe) == 0x14) cmd_ = Cmd::StartHi;
    if ((data & 0xfe) == 0x24) cmd_ = Cmd::EndHi;
    if ((data & 0xfe) == 0x74) { // play
        const uint32_t rom_bytes = uint32_t(mpeg_rom_.size());
        uint32_t start = mp_start_ & 0xffffff, end = mp_end_ & 0xffffff;
        if (rom_bytes > 0x1000000 && rom_bank_) start |= 0x1000000, end |= 0x1000000;
        if (start >= rom_bytes || end > rom_bytes || end <= start) { // MAME refuses an out-of-range window
            playing_ = false;
            audio_pos_ = audio_avail_ = 0;
        } else {
            mp_start_ = start, mp_end_ = end;
            mp_pos_ = int32_t(mp_start_ * 8);
            playing_ = true;
        }
    }
    if ((data & 0xfe) == 0x84) { // stop
        playing_ = false;
        audio_pos_ = audio_avail_ = 0;
    }
}

void Dsb2::update_stream() {
    ++updates_;
    sched_.at(uint64_t(std::llround(double(updates_ + 1) * double(kIps) / 50.0)), [this] { update_stream(); });
    int samples = int(kRate / 50);
    const float scale = 1.0f / (32768.0f * 128.0f);
    for (;;) {
        while (samples && audio_pos_ < audio_avail_) {
            const int16_t l = audio_buf_[audio_pos_ * 2], r = audio_buf_[audio_pos_ * 2 + 1];
            const int16_t sl = mp_pan_ == 2 ? r : l, sr = mp_pan_ == 1 ? l : r;
            if (mp_pan_ <= 2) {
                out_.push_back(float(sl * int32_t(mp_vol_)) * scale);
                out_.push_back(float(sr * int32_t(mp_vol_)) * scale);
                samples--;
            }
            audio_pos_++;
        }
        if (!samples || !playing_) break;
        int sample_rate, channel_count;
        if (decoder_->decode_buffer(mp_pos_, int(mp_end_ * 8), audio_buf_, audio_avail_, sample_rate, channel_count))
            audio_pos_ = 0;
        else
            playing_ = false;
    }
    // MAME's stream gives silence for the samples nothing wrote.
    out_.insert(out_.end(), size_t(2 * samples), 0.0f);
}

} // namespace snd

#endif // M2_DSB2
