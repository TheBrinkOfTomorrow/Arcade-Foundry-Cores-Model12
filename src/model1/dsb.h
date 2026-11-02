// Sega's Digital Sound Board (Star Wars Arcade's music and speech, Sega
// Touring Car Championship's music) as
// native code, after MAME's src/mame/sega/dsbz80.cpp (BSD-3, R. Belmont,
// Olivier Galibert): the Z80 program statically recompiled (dsb_z80.h), its
// i8251 receiving the bytes the sound board's 68000 (Model 1) or the i960
// (Model 2C) sends, its RxRDY on the Z80's interrupt line, the MPEG
// registers, and the layer II decoder (mpeg_audio.h) producing 32 kHz
// stereo.
//
// Like MAME, the decoder only runs when the sound system updates the
// board's stream, 50 times a second of board time, and sees the registers as
// they are then.
#pragma once

#include "model1/dsb_z80.h"
#include "model1/mpeg_audio.h"

#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

namespace m1 {

class Dsb : public dsb::Io {
public:
    static constexpr uint64_t kClock = 4000000;  // the Z80, T-states per second
    static constexpr uint32_t kRate = 32000;     // output samples per second
    static constexpr uint64_t kUpdateCycles = kClock / 50; // MAME's stream updates

    // program: region dsb_cpu (the Z80's ROM); mpeg: region dsb_mpeg.
    Dsb(const std::vector<uint8_t> &program, std::vector<uint8_t> mpeg);

    // A byte on the serial line from the sound board, received (stop bit
    // sampled) at board time `seconds`. Bytes come in time order.
    void receive(uint8_t byte, double seconds);
    // Run the board up to board time `seconds`.
    void advance_to(double seconds);

    double rate() const { return kRate; }
    // Rendered audio since the last take: interleaved stereo floats.
    std::vector<float> take() { return std::exchange(out_, {}); }
    // Log the bytes the Z80 reads and its MPEG register writes, timed in
    // seconds ("0.123456 R f0 01", "0.123457 W e0 01"), as scripts/dsb_tap.lua
    // logs MAME's.
    void set_log(std::FILE *log) { log_ = log; }

    // dsb::Io
    uint8_t in(uint8_t port, uint64_t cycles) override;
    void out(uint8_t port, uint8_t value, uint64_t cycles) override;
    bool irq(uint64_t cycles) override; // the i8251's RxRDY pin (MAME: the Z80's IRQ0)

private:
    void deliver_to(uint64_t cycles);
    void update_stream(); // MAME's sound_stream_update for one update's samples

    dsb::Z80 z_;
    std::vector<uint8_t> mpeg_rom_;
    std::unique_ptr<mpeg_audio> decoder_;

    // i8251, receive side
    std::deque<std::pair<uint64_t, uint8_t>> line_; // (arrival T-state, byte)
    bool rx_enable_ = false, rxrdy_ = false, overrun_ = false, expect_mode_ = false, configured_ = false;
    uint8_t rx_data_ = 0;

    // MPEG registers and playback (dsbz80_device's members)
    uint32_t mp_start_ = 0, mp_end_ = 0, mp_vol_ = 0x7f, mp_pan_ = 0, mp_state_ = 0, lp_start_ = 0, lp_end_ = 0,
             start_ = 0, end_ = 0;
    int32_t mp_pos_ = 0, audio_pos_ = 0, audio_avail_ = 0;
    int16_t audio_buf_[1152 * 2] = {};

    uint64_t next_update_ = kUpdateCycles; // the T-state of the next stream update
    std::vector<float> out_;
    std::FILE *log_ = nullptr;
};

} // namespace m1
