// Sega's Digital Sound Board 2 (Top Skater's music) as native code, after
// MAME's src/mame/sega/dsb2.cpp (BSD-3): its 68000 program statically
// recompiled (tools/m2sndrecomp --dsb2) on its own context, the i8251
// receiving the bytes the i960 sends the sound board (the same line), its
// RxRDY on the 68000's IRQ 1, a 1 kHz timer on IRQ 2 (held until taken), the
// MPEG command FIFO, and the layer II decoder (model1/mpeg_audio.h) producing
// 32 kHz stereo.
//
// Time is counted in completed 68000 instructions at kIps, as on the sound
// board (scsp_board.h). Like MAME, the decoder only runs when the sound
// system updates the board's stream, 50 times a second of board time, and
// sees the FIFO's play window as it is then.
#pragma once

#include "model1/mpeg_audio.h"
#include "runtime/dsb2_gen_support.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

namespace snd {

class Dsb2 : public Devices {
public:
    // 68000 instructions per second of board time at MAME's 8 MHz: the rate
    // that puts the program's UART reads and FIFO writes nearest MAME's
    // (within 10 ms over a race, m2dsb2check).
    static constexpr uint64_t kIps = 870000;
    static constexpr uint32_t kRate = 32000; // output samples per second

    // program: region dsb2:mpegcpu (byte-swapped to big-endian); mpeg: dsb2:mpeg.
    Dsb2(const std::vector<uint8_t> &program, std::vector<uint8_t> mpeg);

    // A byte on the serial line from the i960, received (stop bit sampled) at
    // board time `seconds`. Bytes come in time order.
    void receive(uint8_t byte, double seconds);
    // Run the board up to board time `seconds`.
    void advance_to(double seconds);

    double rate() const { return kRate; }
    // Rendered audio since the last take: interleaved stereo floats.
    std::vector<float> take() { return std::exchange(out_, {}); }
    uint64_t instructions() const { return sched_.count; }
    // Log the bytes the 68000 reads from its UART and its FIFO and control
    // writes, timed in seconds ("0.123456 R c00001 01", "0.123457 W e00003 14"),
    // as the m2trace plugin's M2TRACE_DSBLOG logs MAME's.
    void set_log(std::FILE *log) { log_ = log; }

    // Devices (the 68000's view: its odd-byte devices)
    uint8_t read(uint32_t addr) override;
    void write(uint32_t addr, uint16_t data) override;

private:
    void deliver(uint8_t byte);
    void update_irq();
    void tick_1khz();
    void update_stream();
    void fifo_w(uint8_t data);
    double now() const { return double(sched_.count) / double(kIps); }

    std::vector<uint8_t> program_;
    Cpu68k cpu_;
    Sched sched_{cpu_};
    sndgen::Env env_{cpu_, sched_};
    std::vector<uint8_t> mpeg_rom_;
    std::unique_ptr<m1::mpeg_audio> decoder_;

    // i8251, receive side (as m1::Dsb's)
    bool rx_enable_ = false, rxrdy_ = false, overrun_ = false, expect_mode_ = false, configured_ = false;
    uint8_t rx_data_ = 0;
    bool timer_irq_ = false; // the 1 kHz timer's IRQ 2, held until the 68000 takes it
    uint64_t ms_ = 0;        // 1 kHz ticks so far

    // The FIFO's commands and the player (dsb2_device's members)
    enum class Cmd { Idle, StartHi, StartMd, StartLo, EndHi, EndMd, EndLo } cmd_ = Cmd::Idle;
    uint32_t start_ = 0, end_ = 0, mp_start_ = 0, mp_end_ = 0, mp_vol_ = 0x7f, mp_pan_ = 0, rom_bank_ = 0;
    bool playing_ = false;
    int32_t mp_pos_ = 0, audio_pos_ = 0, audio_avail_ = 0;
    int16_t audio_buf_[1152 * 2] = {};

    uint64_t updates_ = 0; // stream updates so far
    std::vector<float> out_;
    std::FILE *log_ = nullptr;
};

} // namespace snd
