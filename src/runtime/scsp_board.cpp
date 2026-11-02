#include "runtime/scsp_board.h"
#if M2_M1SND
#include "runtime/m1snd_gen_support.h"
#include "runtime/sound_board.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace snd {

ScspBoard::ScspBoard(const std::vector<uint8_t> &program, const std::vector<uint8_t> &samples)
    : program_(program), samples_(samples) {
    if (program_.size() < 0x40000) throw rt::Fatal("sound program image missing or short (re-run the importer)");
    if (samples_.size() < 0x800000) throw rt::Fatal("sample ROM image missing or short (re-run the importer)");
    program_.resize(0x80000); // the program region (512 KB; the ROM fills half)
    samples_.resize(0x800000);
    cpu_.rom = program_.data();
    cpu_.samples = samples_.data();
    cpu_.dev = this;
    std::copy(program_.begin(), program_.begin() + 16, cpu_.ram.begin()); // MAME reset_model2_scsp: the reset vectors
    cpu_.reset();

    scsp_.on_irq = [this] {
        cpu_.irq_line = scsp_.irq_level();
        sched_.poke();
    };
    scsp_.on_timer = [this](int n, double seconds) {
        const uint64_t gen = ++timer_gen_[n];
        const uint64_t at = sched_.count + uint64_t(std::llround(seconds * double(kIps)));
        sched_.at(at, [this, n, gen] {
            if (gen == timer_gen_[n]) scsp_.timer_expired(n);
        });
    };
}

#if M2_DSB
ScspBoard::ScspBoard(const std::vector<uint8_t> &program, const std::vector<uint8_t> &samples,
                     const std::vector<uint8_t> &dsb_program, std::vector<uint8_t> dsb_mpeg)
    : ScspBoard(program, samples) {
    if (!dsb_program.empty()) dsb_ = std::make_unique<DsbBoard>(dsb_program, std::move(dsb_mpeg));
}
#endif

#if M2_M1SND
ScspBoard::ScspBoard(const std::vector<uint8_t> &program, const std::vector<uint8_t> &samples,
                     const std::vector<uint8_t> &m1_program, const std::vector<uint8_t> &m1_pcm1)
    : ScspBoard(program, samples) {
    if (m1_program.empty() || !m1sndgen::native_instructions()) return; // (a core built without its ROMs: none)
    m1_ = std::make_unique<M1SoundBoard>(m1_program, m1_pcm1, std::vector<uint8_t>(0x400000, 0), M1SoundBoard::kIps,
                                         M1SoundBoard::Gen{m1sndgen::has_code, m1sndgen::run});
    m1_fm_.step = m1_->fm_rate() / Scsp::kRate;
    m1_pcm_.step = m1_->pcm_rate() / Scsp::kRate;
}

ScspBoard::~ScspBoard() = default;

void ScspBoard::Resample::run(const std::vector<float> &in, std::vector<float> &out) {
    const size_t n = in.size() / 2;
    for (; pos < double(n); pos += step) { // pos: in input samples, -1 being prev
        const size_t i = size_t(pos);
        const float t = float(pos - double(i));
        for (int ch = 0; ch < 2; ++ch) {
            const float a = i == 0 ? prev[ch] : in[2 * (i - 1) + ch], b = in[2 * i + ch];
            out.push_back(a + (b - a) * t);
        }
    }
    pos -= double(n);
    if (n) prev[0] = in[2 * n - 2], prev[1] = in[2 * n - 1];
}

std::vector<float> ScspBoard::take_fm() {
    std::vector<float> out = std::exchange(out_, {});
    if (!m1_) return out;
    m1_fm_.run(m1_->take_fm(), m1_fm_out_);
    m1_pcm_.run(m1_->take_pcm(), m1_pcm_out_);
    const size_t n = std::min({out.size(), m1_fm_out_.size(), m1_pcm_out_.size()});
    for (size_t i = 0; i < n; ++i) out[i] = std::clamp(out[i] + m1_fm_out_[i] + m1_pcm_out_[i], -1.0f, 1.0f);
    m1_fm_out_.erase(m1_fm_out_.begin(), m1_fm_out_.begin() + std::ptrdiff_t(n));
    m1_pcm_out_.erase(m1_pcm_out_.begin(), m1_pcm_out_.begin() + std::ptrdiff_t(n));
    constexpr size_t kMax = 2 * Scsp::kRate / 10; // a tenth of a second ahead at most (the rates' drift)
    if (m1_fm_out_.size() > kMax) m1_fm_out_.erase(m1_fm_out_.begin(), m1_fm_out_.end() - std::ptrdiff_t(kMax));
    if (m1_pcm_out_.size() > kMax) m1_pcm_out_.erase(m1_pcm_out_.begin(), m1_pcm_out_.end() - std::ptrdiff_t(kMax));
    return out;
}
#endif

void ScspBoard::send(const uint8_t *bytes, size_t n) {
    constexpr uint64_t kByteTime = kIps * kByteBits / kBaud; // instructions per byte on the line
#if M2_M1SND
    if (m1_) m1_->send(bytes, n); // the same line (its own board time)
#endif
    for (size_t i = 0; i < n; ++i) {
        const uint64_t at = std::max(line_free_, sched_.count) + kByteTime;
        line_free_ = at;
        const uint8_t b = bytes[i];
#if M2_DSB
        if (dsb_) dsb_->receive(b, double(at) / double(kIps)); // the same line, its stop bit at the same time
#endif
        sched_.at(at, [this, b] {
            scsp_.midi_in(b);
            ++received_;
        });
    }
}

void ScspBoard::advance(double seconds) {
    const double want = seconds * double(kIps) + carry_;
    const uint64_t n = uint64_t(want);
    carry_ = want - double(n);
    run_to(sched_.count + n);
#if M2_M1SND
    if (m1_) m1_->advance(seconds);
#endif
}

void ScspBoard::run_to(uint64_t count) {
    while (sched_.count < count) {
        sched_.end_count = count;
        if (!sndgen::has_code(cpu_.pc)) {
            char b[96];
            std::snprintf(b, sizeof b, "sound 68000: no recompiled code at %06x", cpu_.pc);
            throw rt::Fatal(b);
        }
        sndgen::run(env_);
    }
    render_to(count);
    scsp_.take_midi_out(); // the i960 never reads its UART (as in MAME)
#if M2_DSB
    if (dsb_) dsb_->advance_to(double(count) / double(kIps));
#endif
}

void ScspBoard::render_to(uint64_t count) {
    const uint64_t due = count * Scsp::kRate / kIps;
    if (due <= rendered_) return;
    const size_t n = size_t(due - rendered_), at = out_.size();
    out_.resize(at + 2 * n);
    scsp_.render(out_.data() + at, int(n));
    rendered_ = due;
}

uint16_t ScspBoard::read16(uint32_t addr, uint16_t) {
    if (addr >= 0x100000 && addr <= 0x100fff) {
        render_to(sched_.count);
        return scsp_.read(addr - 0x100000);
    }
    return 0;
}

void ScspBoard::write16(uint32_t addr, uint16_t data, uint16_t mask) {
    if (addr >= 0x100000 && addr <= 0x100fff) {
        render_to(sched_.count);
        scsp_.write(addr - 0x100000, data, mask);
    }
    // 0x400000: the sample banks (MAME model2snd_ctrl): only sets with more
    // than 8 MB of samples bank them; Sega Rally's 8 MB are mapped flat
}

} // namespace snd
