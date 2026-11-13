// The Model 2A-CRX sound board as native runtime (design doc, sound): the
// recompiled 68000 program (tools/m2sndrecomp --model2a) on its native
// context, and the SCSP (MAME's, transplanted: scsp.h) receiving the i960's
// command bytes on its MIDI input. MAME's model2_snd map: sound RAM at 0
// (shared with the SCSP), the SCSP's registers at 0x100000, the program at
// 0x600000, the sample ROMs at 0x800000.
//
// No cycle clock, as on the Model 1 board: time is counted in completed
// 68000 instructions at the rate MAME's 68000 runs this program (kIps), and
// events land on that count: a command byte at the MIDI line rate, an SCSP
// timer expiring. The SCSP is rendered up to the current count before each
// register access, so every write takes effect on the right sample.
//
// The interface is the Model 1 board's (sound_board.h), so the game loop and
// the app take either: the SCSP's one stereo output comes out of take_fm(),
// take_pcm() is empty, both at 44.1 kHz.
//
// Model 2C's Touring Car (a Digital Sound Board image given): the i960's
// UART line also reaches the Digital Sound Board (m1::Dsb, its Z80 program
// recompiled), whose MPEG music comes out of take_pcm() at its 32 kHz.
// Top Skater's (M2_DSB2) is the Digital Sound Board 2 (snd::Dsb2, a 68000),
// in the same place.
//
// Manx TT's build (M2_M1SND, a Model 1 sound board image given): the line
// also reaches a Model 1 sound board (M1SoundBoard, sound_board.h), which
// plays the engine and road sounds; its output is resampled to the SCSP's
// rate and mixed into take_fm() at the board's own gains, as MAME's
// manxttdx mixes its segam1audio.
#pragma once

#include "runtime/scsp.h"
#include "runtime/snd_gen_support.h"
#if M2_DSB2
#include "runtime/dsb2.h"
#elif M2_DSB
#include "model1/dsb.h"
#endif

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace snd {

#if M2_M1SND
class M1SoundBoard;
#endif

#if M2_DSB2
using DsbBoard = Dsb2;
#elif M2_DSB
using DsbBoard = m1::Dsb;
#endif

class ScspBoard : public Devices {
public:
    // 68000 instructions per second of board time: MAME's 68000 (11.2896
    // MHz) runs 46,585,675 instructions of this program in 60 s of attract.
    static constexpr uint64_t kIps = 776428;
    // The app opens its streams at kYmClock / 144 and kPcmClock / 224: both
    // the SCSP's rate here.
    static constexpr uint32_t kYmClock = Scsp::kRate * 144, kPcmClock = Scsp::kRate * 224;
    static constexpr uint32_t kBaud = 31250;  // MIDI-rate serial line from the i960
    static constexpr uint32_t kByteBits = 10; // start, 8 data, stop

    ScspBoard(const std::vector<uint8_t> &program, const std::vector<uint8_t> &samples);
#if M2_DSB
    // With the Digital Sound Board (dsb_program empty: none).
    ScspBoard(const std::vector<uint8_t> &program, const std::vector<uint8_t> &samples,
              const std::vector<uint8_t> &dsb_program, std::vector<uint8_t> dsb_mpeg);
#endif

#if M2_M1SND
    // With the Model 1 sound board (m1_program empty: none).
    ScspBoard(const std::vector<uint8_t> &program, const std::vector<uint8_t> &samples,
              const std::vector<uint8_t> &m1_program, const std::vector<uint8_t> &m1_pcm1);
    ~ScspBoard();
#endif

    // Command bytes the i960 wrote to its UART; they arrive one line-time
    // apart from now on.
    void send(const uint8_t *bytes, size_t n);
    // Run the board for `seconds` of board time.
    void advance(double seconds);

    double fm_rate() const { return Scsp::kRate; }
#if M2_DSB
    double pcm_rate() const { return dsb_ ? dsb_->rate() : Scsp::kRate; }
    std::vector<float> take_pcm() { return dsb_ ? dsb_->take() : std::vector<float>{}; } // the DSB, interleaved stereo
    DsbBoard *dsb() { return dsb_.get(); }
#else
    double pcm_rate() const { return Scsp::kRate; }
    std::vector<float> take_pcm() { return {}; }
#endif
#if M2_M1SND
    std::vector<float> take_fm(); // the SCSP and the Model 1 board, interleaved stereo
#else
    std::vector<float> take_fm() { return std::exchange(out_, {}); } // the SCSP, interleaved stereo
#endif

    uint64_t instructions() const { return sched_.count; }
    // The launcher's music and effects volumes: Daytona's only (the SCSP
    // mixes them itself); the app's overall volume still applies.
    void set_volumes(float, float) {}
    int voice_channel(unsigned, uint8_t) const { return -1; }
    const std::vector<uint8_t> &ram() const { return cpu_.ram; }
    size_t bytes_received() const { return received_; }
    void state(rt::StateIO &s); // save states

    // Devices (the 68000's view)
    enum : uint32_t { kMidiEvent = 1, kTimerEvent = 2 }; // Sched tags
    std::function<void()> event(uint32_t tag, uint64_t arg);

public:
    uint16_t read16(uint32_t addr, uint16_t mask) override;
    void write16(uint32_t addr, uint16_t data, uint16_t mask) override;

private:
    void run_to(uint64_t count);
    void render_to(uint64_t count);

    std::vector<uint8_t> program_, samples_;
    Cpu68k cpu_;
    Sched sched_{cpu_};
    sndgen::Env env_{cpu_, sched_};
    Scsp scsp_{cpu_.ram};
    double carry_ = 0; // fractional instructions from advance()

    uint64_t line_free_ = 0; // count at which the serial line is free for the next byte
    size_t received_ = 0;
    uint64_t timer_gen_[3] = {};

    uint64_t rendered_ = 0; // samples
    std::vector<float> out_;
#if M2_DSB
    std::unique_ptr<DsbBoard> dsb_;
#endif
#if M2_M1SND
    std::unique_ptr<M1SoundBoard> m1_;
    struct Resample { // linear, interleaved stereo, from one rate to the SCSP's
        double step = 1, pos = 0;
        float prev[2] = {};
        void run(const std::vector<float> &in, std::vector<float> &out);
    } m1_fm_, m1_pcm_;
    std::vector<float> m1_fm_out_, m1_pcm_out_; // resampled, not yet mixed
#endif
};

} // namespace snd
