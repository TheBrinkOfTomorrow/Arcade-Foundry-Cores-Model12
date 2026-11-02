// The Digital Sound Board (dsb.h). The register semantics and the stream
// update are MAME's dsbz80.cpp (license: BSD-3-Clause, copyright-holders:
// R. Belmont, Olivier Galibert), with the device framework replaced by plain
// members.
#include "model1/dsb.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace m1 {

Dsb::Dsb(const std::vector<uint8_t> &program, std::vector<uint8_t> mpeg) : mpeg_rom_(std::move(mpeg)) {
    if (program.size() < 0x8000) throw std::runtime_error("DSB: no Z80 program");
    std::copy_n(program.begin(), 0x8000, z_.mem);
    z_.io = this;
    decoder_ = std::make_unique<mpeg_audio>(mpeg_rom_.data(), mpeg_audio::L2, false, 0);
}

void Dsb::receive(uint8_t byte, double seconds) {
    line_.emplace_back(uint64_t(std::llround(seconds * double(kClock))), byte);
}

void Dsb::deliver_to(uint64_t cycles) {
    while (!line_.empty() && line_.front().first <= cycles) {
        const uint8_t b = line_.front().second;
        line_.pop_front();
        if (!rx_enable_) continue;
        if (rxrdy_) overrun_ = true;
        rx_data_ = b, rxrdy_ = true;
    }
}

void Dsb::advance_to(double seconds) {
    const uint64_t until = uint64_t(seconds * double(kClock));
    while (z_.cycles < until) {
        const uint64_t stop = std::min(until, next_update_);
        // Bytes due before the stop are latched as the Z80 reads the UART
        // (in()), so the run needs no other break.
        dsb::run(z_, stop);
        if (z_.cycles >= next_update_) {
            update_stream();
            next_update_ += kUpdateCycles;
        }
    }
}

uint8_t Dsb::in(uint8_t port, uint64_t cycles) {
    deliver_to(cycles);
    switch (port) {
    case 0xf0: // i8251 data
        rxrdy_ = false;
        if (log_) std::fprintf(log_, "%.6f R f0 %02x\n", double(cycles) / kClock, rx_data_);
        return rx_data_;
    case 0xf1: // i8251 status: DSR, TxEMPTY, TxRDY; RxRDY; overrun
        return uint8_t(0x85 | (rxrdy_ ? 0x02 : 0) | (overrun_ ? 0x10 : 0));
    case 0xe2: return uint8_t((mp_pos_ >> 3) >> 16);
    case 0xe3: return uint8_t((mp_pos_ >> 3) >> 8);
    case 0xe4: return uint8_t(mp_pos_ >> 3);
    default: return 0xff;
    }
}

bool Dsb::irq(uint64_t cycles) { // i8251 rxrdy_r: RxRDY, masked by RxE
    deliver_to(cycles);
    return rx_enable_ && rxrdy_;
}

void Dsb::out(uint8_t port, uint8_t value, uint64_t cycles) {
    deliver_to(cycles);
    if (log_ && port >= 0xe0 && port <= 0xe9) std::fprintf(log_, "%.6f W %02x %02x\n", double(cycles) / kClock, port, value);
    switch (port) {
    case 0xe0: // trigger (mpeg_trigger_w)
        mp_state_ = value;
        if (value == 0) mp_state_ = 0, audio_pos_ = audio_avail_ = 0;
        else if (value == 1 || value == 2) mp_pos_ = int32_t(mp_start_ * 8);
        break;
    case 0xe2: case 0xe3: case 0xe4: { // start (mpeg_start_w)
        const int shift = 8 * (0xe4 - port);
        start_ = (start_ & ~(0xffu << shift)) | uint32_t(value) << shift;
        if (port == 0xe4) {
            if (mp_state_ == 0) mp_start_ = start_;
            else lp_start_ = start_;
        }
        break;
    }
    case 0xe5: case 0xe6: case 0xe7: { // end (mpeg_end_w)
        const int shift = 8 * (0xe7 - port);
        end_ = (end_ & ~(0xffu << shift)) | uint32_t(value) << shift;
        if (port == 0xe7) {
            if (mp_state_ == 0) mp_end_ = end_;
            else lp_end_ = end_;
        }
        break;
    }
    case 0xe8: mp_vol_ = ~value & 0x7f; break; // the MSB's purpose is unknown
    case 0xe9: mp_pan_ = value & 3; break;     // 0 stereo, 1 left on both, 2 right on both
    case 0xf1: // i8251 mode / command
        if (expect_mode_) {
            expect_mode_ = false;
        } else if (value & 0x40) { // internal reset: a mode instruction follows
            expect_mode_ = configured_ = true, rx_enable_ = false;
        } else if (configured_) {
            rx_enable_ = value & 0x04;
            if (value & 0x10) overrun_ = false; // error reset
        }
        break;
    default: break; // EA, EB: no device in MAME
    }
}

void Dsb::update_stream() {
    int samples = int(kRate * kUpdateCycles / kClock);
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
        if (!samples || mp_state_ == 0) break;
        int sample_rate, channel_count;
        if (decoder_->decode_buffer(mp_pos_, int(mp_end_ * 8), audio_buf_, audio_avail_, sample_rate, channel_count)) {
            audio_pos_ = 0;
        } else if (mp_state_ == 2) {
            if (mp_pos_ == int32_t(lp_start_ * 8)) mp_state_ = 0; // looping on undecodable data
            mp_pos_ = int32_t(lp_start_ * 8);
            if (lp_end_) mp_end_ = lp_end_;
        } else {
            mp_state_ = 0;
        }
    }
    // MAME's stream gives silence for the samples nothing wrote.
    out_.insert(out_.end(), size_t(2 * samples), 0.0f);
}

} // namespace m1
