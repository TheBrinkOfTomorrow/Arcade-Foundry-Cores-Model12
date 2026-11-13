#include "af_debug.h"
#include "runtime/game_loop.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

namespace rt {

namespace {
std::vector<uint8_t> load(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path + " (run the setup / importer first)");
    return {std::istreambuf_iterator<char>(f), {}};
}

constexpr uint64_t kProbe = 1024;       // instructions between wait-loop checks
constexpr uint64_t kFrameCap = 110000;  // a CPU-bound frame: 25 MHz / 57.52 Hz of i960 work, as MAME measures
constexpr uint64_t kVblankCap = 40000;  // a vblank handler that never returns to the wait loop
constexpr uint64_t kMinFrame = kProbe * 2;
} // namespace

GameLoop::GameLoop(const std::string &dir, bool sound_enabled) : GameLoop([&] {
    M2Board::Images img;
    img.program = load(dir + "/program.bin");
    img.main_data = load(dir + "/main_data.bin");
    img.copro_tables = load(dir + "/copro_tables.bin");
    img.copro_data = load(dir + "/copro_data.bin");
    img.polygons = load(dir + "/polygons.bin");
    img.textures = load(dir + "/textures.bin");
    img.sound_program = load(dir + "/sound_program.bin");
    if (kModel2a) {
        img.samples = load(dir + "/samples.bin");
        if (M2_M1SND && std::ifstream(dir + "/m1_program.bin")) { // Manx TT's Model 1 sound board (optional)
            img.m1_program = load(dir + "/m1_program.bin");
            img.pcm1 = load(dir + "/m1_pcm1.bin");
        }
        if (M2_DSB) { // the Digital Sound Board
            img.dsb_program = load(dir + "/dsb_program.bin");
            img.dsb_mpeg = load(dir + "/dsb_mpeg.bin");
        }
    } else {
        img.pcm1 = load(dir + "/pcm1.bin");
        img.pcm2 = load(dir + "/pcm2.bin");
    }
    return img;
}(), sound_enabled) {}

GameLoop::GameLoop(M2Board::Images img, bool sound_enabled) {
    if (sound_enabled && !img.sound_program.empty())
#if M2_MODEL2A
#if M2_DSB
        sound_ = std::make_unique<snd::SoundBoard>(img.sound_program, img.samples, img.dsb_program, img.dsb_mpeg);
#elif M2_M1SND
        sound_ = std::make_unique<snd::SoundBoard>(img.sound_program, img.samples, img.m1_program, img.pcm1);
#else
        sound_ = std::make_unique<snd::SoundBoard>(img.sound_program, img.samples);
#endif
#else
        sound_ = std::make_unique<snd::SoundBoard>(img.sound_program, img.pcm1, img.pcm2);
#endif
    board_ = std::make_unique<M2Board>(std::move(img));
#if M2_FALLBACK_INTERP
    cpu_ = std::make_unique<I960Ref>(board_.get());
#else
    cpu_ = std::make_unique<Cpu>(board_.get());
#endif
    if (const char *v = af_debug_env("M2_STRICT")) strict_ = *v && std::strcmp(v, "0") != 0;
    ls_ = std::make_unique<Lockstep>(*cpu_);
    board_->attach(*cpu_, *ls_);
    cpu_->reset();
    env_ = std::make_unique<gen::Env>(gen::Env{*cpu_, *ls_});
    ls_->add_callback(kProbe, [this] { probe(); }, kProbeTag);
}

void GameLoop::probe() {
    if (kModel2a) board_->fire_timers();
    const bool idle = board_->in_idle_loop();
    if (in_vblank_) {
        if ((idle && ls_->count - vblank_start_ >= kProbe * 2) || ls_->count - vblank_start_ >= kVblankCap) {
            {
                auto sample = profiler_.measure(profiler_.frame.video);
                board_->vblank_end();
            }
            in_vblank_ = false;
            frame_start_ = ls_->count;
            ++frames_;
            frame_done_ = true;
#ifdef M2_FAST_GEN
            ls_->set_end(ls_->count); // gen::run returns to the caller
#else
            ls_->end_count = ls_->count; // gen::run returns to the caller
#endif
        }
    } else {
        const uint64_t since = ls_->count - frame_start_;
        if (kModel2a && idle && since >= kMinFrame && since < kFrameCap) {
            // Waiting for the frame: board time moves on to the next timer
            // the game has an interrupt for, if that comes first, and the
            // game runs its handler (Sega Rally's 500 Hz timer 0 and the
            // per-frame timer 2); else to the end of the frame.
            const uint64_t due = board_->next_timer_due();
            if (due < next_vblank_tick_) {
                board_->skip_to(due);
                board_->fire_timers();
                ls_->add_callback(ls_->count + kProbe, [this] { probe(); }, kProbeTag);
                return;
            }
            board_->skip_to(next_vblank_tick_);
        }
        if ((idle && since >= kMinFrame) || since >= kFrameCap) {
            if (kModel2a) next_vblank_tick_ = std::max(board_->now_ticks(), next_vblank_tick_) + kFrameTicks;
            board_->io().inputs = inputs_;
            {
                auto sample = profiler_.measure(profiler_.frame.geometry);
                board_->vblank_start();
            }
            in_vblank_ = true;
            vblank_start_ = ls_->count;
        }
    }
    ls_->add_callback(ls_->count + kProbe, [this] { probe(); }, kProbeTag);
}

void GameLoop::run_frame(const Inputs &inputs) {
    run_frame_deferred_sound(inputs);
    complete_deferred_sound(execute_deferred_sound());
}

GameLoop::SoundPacket GameLoop::run_frame_sound_packet(const Inputs &inputs) {
    run_frame_deferred_sound(inputs);
    sound_frame_pending_ = false;
    return SoundPacket(sound_.get(), std::move(pending_sound_bytes_), sound_profile_clock_);
}

uint64_t GameLoop::SoundPacket::execute() {
    if (!sound_) return 0;
    if (!pending_) throw Fatal("detached sound frame has already executed");
    // A partial failure must not permit replaying UART bytes or board time.
    pending_ = false;
    const uint64_t begin = clock_ ? clock_() : 0;
    sound_->send(bytes_.data(), bytes_.size());
    sound_->advance(1.0 / kFrameHz);
    const uint64_t end = clock_ ? clock_() : 0;
    return end >= begin ? end - begin : 0;
}

void GameLoop::state(StateIO &s) {
    s.mark("LOOP");
    if (sound_frame_pending_) return s.fail("a sound frame is still running");
    s(inputs_), s(in_vblank_), s(frame_done_), s(frame_start_), s(vblank_start_), s(frames_), s(next_vblank_tick_);
    s(cabinet_setup_), s(interpreted_);
    s.mark("I960");
    cpu_->state(s);
    ls_->state(s, [this](int tag) -> std::function<void()> {
        if (tag == kProbeTag) return [this] { probe(); };
        return board_->lockstep_callback(tag);
    });
    board_->state(s);
    bool sound = sound_ != nullptr;
    s(sound);
    if (sound != (sound_ != nullptr)) return s.fail("the save state was made with the sound board on or off, not as now");
    if (sound_) sound_->state(s);
    s.mark("END ");
}

void GameLoop::set_cabinet_setup(bool on) {
    const auto &e = board_->io().eeprom;
    cabinet_setup_ = on && kManxTT && frames_ == 0 && std::all_of(e.begin(), e.end(), [](uint8_t b) { return b == 0xff; });
}

void GameLoop::cabinet_setup(Inputs &in) const {
    if (frames_ >= 1500) return;
    // Each press 4 frames, 20 apart. The test menu opens on EXIT; SERVICE
    // moves down (wrapping), TEST selects or changes the setting.
    struct Step { uint32_t frame; uint8_t bit; };
    static const Step steps[] = {
        {650, 0x04},                                                                          // test mode
        {700, 0x08}, {720, 0x08}, {740, 0x08}, {760, 0x08}, {780, 0x08}, {820, 0x04},         // GAME ASSIGNMENTS
        {880, 0x08}, {900, 0x08}, {920, 0x08}, {960, 0x04},                                   // CABINET TYPE: TWIN
        {1020, 0x08}, {1040, 0x08}, {1060, 0x08}, {1080, 0x08}, {1100, 0x08}, {1120, 0x08},
        {1140, 0x08}, {1160, 0x08}, {1180, 0x08}, {1200, 0x08}, {1220, 0x08}, {1260, 0x04}, // EXIT
        {1320, 0x08}, {1340, 0x08}, {1360, 0x08}, {1380, 0x08}, {1400, 0x08}, {1440, 0x04}, // EXIT: the game restarts
    };
    for (const Step &s : steps)
        if (frames_ >= s.frame && frames_ < s.frame + 4) in.in0 &= uint8_t(~s.bit);
}

void GameLoop::run_frame_deferred_sound(const Inputs &inputs) {
    if (sound_frame_pending_) throw Fatal("previous sound frame must complete before advancing the board");
    board_->scenery()->measured = {};
    profiler_.reset();
    auto frame_sample = profiler_.measure(profiler_.frame.total);
    inputs_ = inputs;
    if (cabinet_setup_) cabinet_setup(inputs_);
    frame_done_ = false;
#ifdef M2_FAST_GEN
    ls_->set_end(UINT64_MAX);
#else
    ls_->end_count = UINT64_MAX;
#endif
    while (!frame_done_) {
#ifdef M2_FAST_GEN
        // has_code is a binary search over every address: asked only after a
        // run that did nothing (where the IP has no code, run returns at once).
        const uint64_t count = ls_->count;
        const uint32_t ip = cpu_->m_IP;
        gen::run(*env_);
        if (ls_->count == count && cpu_->m_IP == ip && !frame_done_ && !gen::has_code(ip)) {
#else
        if (!gen::has_code(cpu_->m_IP)) {
#endif
            no_code();
            continue;
        }
#ifndef M2_FAST_GEN
        gen::run(*env_);
#endif
    }
    // Transfer the UART bytes on the owning thread. The sound worker never
    // touches M2Board, video, i960/TGP state, or the frame profiler.
    if (sound_) {
        pending_sound_bytes_ = board_->take_sound_bytes();
        sound_frame_pending_ = true;
    }
}

// Code the recompiler did not reach. The fallback interpreter (MAME's i960
// interpreter, src/refcore/i960_ref) runs it instruction by instruction with
// the same lockstep bookkeeping as generated code (events and interrupts
// before each instruction, the count after it) until control is back at an
// address with recompiled code or the frame ends. Each new entry address is
// logged: it is a seed for the next recompile (rules.md). With M2_STRICT=1,
// or in a build without the interpreter, it is a hard error naming the address.
void GameLoop::no_code() {
    const uint32_t ip = cpu_->m_IP;
#if M2_FALLBACK_INTERP
    if (!strict_) {
        if (fallback_seen_.insert(ip).second)
            std::fprintf(stderr, "i960: fallback interpreter at %08x (frame %llu; a seed for the next recompile)\n", ip,
                         (unsigned long long)frames_);
        do {
            const bool stop = ls_->boundary(); // may take an interrupt (IP changes) or end the frame
            if (ls_->finished()) return;
            if (stop) continue;
            cpu_->execute_one();
            ++ls_->count;
            ++interpreted_;
        } while (!gen::has_code(cpu_->m_IP));
        return;
    }
#endif
    char b[128];
    std::snprintf(b, sizeof b, "no recompiled code at %08x: add it to the seeds", ip);
    throw Fatal(b);
}

uint64_t GameLoop::execute_deferred_sound() {
    if (!sound_frame_pending_) return 0;
    const uint64_t begin = sound_profile_clock_ ? sound_profile_clock_() : 0;
    sound_->send(pending_sound_bytes_.data(), pending_sound_bytes_.size());
    sound_->advance(1.0 / kFrameHz);
    const uint64_t end = sound_profile_clock_ ? sound_profile_clock_() : 0;
    return end >= begin ? end - begin : 0;
}

void GameLoop::complete_deferred_sound(uint64_t ticks) {
    if (!sound_frame_pending_) return;
    pending_sound_bytes_.clear();
    sound_frame_pending_ = false;
    profiler_.frame.sound += ticks;
    // Total represents cumulative work, not elapsed time when overlapped.
    // Adding the same duration to total and sound keeps core() unchanged.
    profiler_.frame.total += ticks;
}

} // namespace rt
