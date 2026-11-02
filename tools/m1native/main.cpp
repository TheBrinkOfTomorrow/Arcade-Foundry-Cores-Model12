// m1native: the Model 1 M1 lockstep harness for recompiled V60 code. The
// same checks as m1replay (every device access, interrupt and sample against
// the MAME trace), but the V60 code is the native C++ m1recomp generated.
// There is no fallback: control reaching an address with no recompiled code
// is a hard error naming it.
//
//   m1native ROMSET TRACE.m1tr [--set vr] [--check] [--frames FRAMES.txt]
//            [--dump DIR --every N] [--inputs SCRIPT] [--sound OUT.wav]
//
// --check also runs the interpreter on its own bus, one instruction at a
// time beside the native code, and stops at the first register difference.
// --frames (MAME's M1TRACE_FRAMES log, patch 0006): the video is native too;
// at each of MAME's vblanks and screen updates (by V60 instruction count)
// ours runs, and each frame's hashes are compared with MAME's (measured).
// --inputs: the controls script MAME's run had (scripts/m1_inputs.lua), set
// on our I/O board at each vblank (needs --frames).
// --sound: the sound board native too (needs --frames): the bytes the V60
// sends its UART drive the recompiled 68000 and the chips, on MAME's time
// (each vblank is a frame of MAME's 57.5242 Hz screen); the audio is written
// as a 48 kHz WAV to compare with MAME's (-wavwrite).
// The I/O board is native (M1_REPLAY_IO=1 replays it); its reads are measured.
// --dump writes every Nth frame of ours as raw RGBA (ours_scr_NNNNN.rgb).

#include "model1/io_board.h"
#include "../common/m1_inputs.h"
#include "model1/native.h"
#include "model1/replay_bus.h"
#include "model1/rom_set.h"
#include "model1/tgp.h"
#include "model1/video.h"
#if M1_NATIVE_SOUND
#include "runtime/sound_board.h"
#endif
#include "v60/interp.h"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace m1 {
void NativeEnv::miss(v60::Core &c) {
    char b[128];
    std::snprintf(b, sizeof b, "no recompiled code at %06x (instruction %" PRIu64 "): add it to the seeds",
                  c.m_reg[32] & 0xffffff, count);
    throw v60::Fatal(b);
}
} // namespace m1

namespace {
// One of MAME's video events, in order: a vblank or a screen update.
struct FrameEvent {
    bool update = false;
    uint64_t frame = 0, count = 0;
    uint64_t bg = 0, d3 = 0, scr = 0;
};

std::vector<FrameEvent> read_frames(const std::string &path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot read " + path);
    std::vector<FrameEvent> out;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream s(line);
        std::string k;
        s >> k;
        if (k == "vb" || k == "su") {
            FrameEvent e;
            e.update = k == "su";
            s >> e.frame >> e.count;
            out.push_back(e);
        } else if ((k == "bg" || k == "3d" || k == "scr") && !out.empty()) {
            std::string h;
            s >> h;
            const uint64_t v = std::stoull(h, nullptr, 16);
            (k == "bg" ? out.back().bg : k == "3d" ? out.back().d3 : out.back().scr) = v;
        }
    }
    return out;
}
#if M1_NATIVE_SOUND
constexpr double kRefresh = 16000000.0 / (656 * 424); // MAME's screen

// The V60's UART writes to the sound board, at board time.
struct SoundLink : m1::DeviceModel {
    snd::SoundBoard board;
    std::function<double()> now;
    double done = 0;
    std::vector<float> fm, pcm;
    SoundLink(const std::vector<uint8_t> &prog, const std::vector<uint8_t> &p1, const std::vector<uint8_t> &p2, uint64_t ips)
        : board(prog, p1, p2, ips) {}
    void run_to(double t) {
        if (t > done) board.advance(t - done), done = t;
        auto f = board.take_fm(), p = board.take_pcm();
        fm.insert(fm.end(), f.begin(), f.end());
        pcm.insert(pcm.end(), p.begin(), p.end());
    }
    uint32_t read(uint32_t, unsigned) override { return 0; }
    void write(uint32_t a, uint32_t v, unsigned) override {
        if (a != 0xc40000) return; // C40002: the UART's mode and command
        run_to(now());
        const uint8_t b = uint8_t(v);
        board.send(&b, 1);
    }
    // Both streams resampled (linearly) to 48 kHz, mixed, as 16-bit PCM.
    void write_wav(const std::string &path) const {
        const double rf = board.fm_rate(), rp = board.pcm_rate();
        const size_t n = size_t(std::min(double(fm.size() / 2) / rf, double(pcm.size() / 2) / rp) * 48000);
        std::vector<int16_t> out(2 * n);
        auto at = [](const std::vector<float> &v, double pos, int ch) {
            const size_t i = size_t(pos);
            const double f = pos - double(i);
            const float a = v[2 * i + ch], b = 2 * (i + 1) + ch < v.size() ? v[2 * (i + 1) + ch] : a;
            return float(a + (b - a) * f);
        };
        for (size_t i = 0; i < n; i++)
            for (int ch = 0; ch < 2; ch++) {
                const double t = double(i) / 48000;
                const float x = at(fm, t * rf, ch) + at(pcm, t * rp, ch);
                out[2 * i + ch] = int16_t(std::clamp(x, -1.0f, 1.0f) * 32767);
            }
        std::ofstream o(path, std::ios::binary);
        auto u32 = [&](uint32_t x) { o.write(reinterpret_cast<const char *>(&x), 4); };
        auto u16 = [&](uint16_t x) { o.write(reinterpret_cast<const char *>(&x), 2); };
        const uint32_t bytes = uint32_t(out.size() * 2);
        o.write("RIFF", 4), u32(36 + bytes), o.write("WAVEfmt ", 8), u32(16), u16(1), u16(2), u32(48000),
            u32(48000 * 4), u16(4), u16(16), o.write("data", 4), u32(bytes);
        o.write(reinterpret_cast<const char *>(out.data()), bytes);
    }
};
#endif
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: m1native ROMSET TRACE.m1tr [--set NAME]\n");
        return 2;
    }
    std::string set_name = "vr";
    bool check = false;
    std::string frames_path, dump_dir, inputs_path, sound_path;
    int dump_every = 0;
    for (int i = 3; i < argc; i++) {
        if (!std::strcmp(argv[i], "--set") && i + 1 < argc) set_name = argv[++i];
        else if (!std::strcmp(argv[i], "--check")) check = true;
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) frames_path = argv[++i];
        else if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) dump_dir = argv[++i];
        else if (!std::strcmp(argv[i], "--every") && i + 1 < argc) dump_every = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--inputs") && i + 1 < argc) inputs_path = argv[++i];
        else if (!std::strcmp(argv[i], "--sound") && i + 1 < argc) sound_path = argv[++i];
    }
    m1::NativeEnv env;
    try {
        const m1::RomSet *set = m1::find_rom_set(set_name);
        if (!set) throw std::runtime_error("unknown set " + set_name);
        m1::Images img = m1::load_rom_set(*set, argv[1]);
        std::vector<uint8_t> rom = img.at("maincpu");
        m1::Memory mem(std::move(img.at("maincpu")));
        auto trace = m1::read_trace(argv[2]);
        m1::Memory ref_mem(std::move(rom));
        m1::ReplayBus ref_bus(ref_mem, check ? trace : std::vector<m1::TraceEvent>{});
        m1::ReplayBus bus(mem, std::move(trace));
        v60::Core ref(&ref_bus);
        uint64_t ref_count = 0;
        if (check) {
            ref_bus.attach(&ref);
            ref_bus.set_counter(&ref_count);
            ref.reset();
            ref_bus.take_init(ref.m_reg);
        }
        v60::Core core(&bus);
        bus.attach(&core);
#if M1_NATIVE_TGP
        // milestone 2: the coprocessor board and the recompiled TGP native
        m1::Copro copro(img.at("tgp_copro"), img.at("copro_tables"), img.at("copro_data"));
        if (!getenv("M1_REPLAY_TGP")) bus.attach_copro(&copro);
        // M1_TGP_TIMED=1: the timed TGP (the core's, src/model1/board): copro
        // RAM reads and each FIFO read's wait against MAME's retries.
        // M1_TGP_SLICE: its timeslice in ticks.
        uint64_t stall_ticks = 0;
        const bool timed = getenv("M1_TGP_TIMED") != nullptr;
        if (getenv("M1_TGP_SLICE")) copro.set_slice(std::strtoull(getenv("M1_TGP_SLICE"), nullptr, 0));
        if (timed)
            copro.set_clock([&] { return env.count * 8 * m1::Copro::kTicksPerV60Cycle + (stall_ticks += copro.take_stall()); });
#endif
        // milestone 3: the I/O board native (controls at rest, as in MAME's run)
        m1::IoBoard io(img.at("eeprom"));
        io.controls = m1::Controls::at_rest(set_name);
        if (!getenv("M1_REPLAY_IO")) bus.attach_io(&io);
        const std::vector<m1tools::InputLine> script = inputs_path.empty() ? std::vector<m1tools::InputLine>{} : m1tools::read_inputs(inputs_path);
        // milestone 2: the video native, checked frame by frame
        std::unique_ptr<m1::Video> video;
        std::vector<FrameEvent> frames;
        size_t next_frame = 0;
        uint64_t updates = 0, same[3] = {0, 0, 0};
        std::string first_diff[3];
        if (!frames_path.empty()) {
            frames = read_frames(frames_path);
            video = std::make_unique<m1::Video>(mem, img.at("polygons"));
            bus.attach_listctl(video.get());
        }
#if M1_NATIVE_SOUND
        // milestone 4: the sound board native
        std::unique_ptr<SoundLink> sound;
        uint64_t vb_frame = 0, vb_count = 0, frame_instructions = 34768;
        if (!sound_path.empty() && video) {
            sound = std::make_unique<SoundLink>(img.at("m1audio"), img.at("mpcm1"), img.at("mpcm2"), set->sound_ips);
            sound->now = [&] {
                return (double(vb_frame) + double(env.count - vb_count) / double(frame_instructions)) / kRefresh;
            };
            bus.attach_sound(sound.get());
        }
#endif
        auto video_events = [&] {
            while (next_frame < frames.size() && frames[next_frame].count == env.count) {
                const FrameEvent &f = frames[next_frame++];
                if (!f.update) {
                    if (!script.empty()) io.controls = m1tools::controls_at(script, f.frame, set_name);
#if M1_NATIVE_SOUND
                    if (sound) {
                        if (f.frame > vb_frame && env.count > vb_count)
                            frame_instructions = (env.count - vb_count) / (f.frame - vb_frame);
                        vb_frame = f.frame, vb_count = env.count;
                        sound->run_to(double(f.frame) / kRefresh);
                    }
#endif
                    video->vblank(f.frame);
                    continue;
                }
                video->screen_update(f.frame);
                ++updates;
                const uint64_t ours[3] = {video->hash_bg, video->hash_3d, video->hash_scr};
                const uint64_t mame[3] = {f.bg, f.d3, f.scr};
                for (int k = 0; k < 3; k++) {
                    if (ours[k] == mame[k]) ++same[k];
                    else if (first_diff[k].empty()) first_diff[k] = "frame " + std::to_string(f.frame);
                }
                if (!dump_dir.empty() && dump_every > 0 && f.frame % uint64_t(dump_every) == 0) {
                    char path[512];
                    std::snprintf(path, sizeof path, "%s/ours_scr_%05u.rgb", dump_dir.c_str(), unsigned(f.frame));
                    if (FILE *d = std::fopen(path, "wb")) {
                        std::fwrite(video->screen().data(), 4, video->screen().size(), d);
                        std::fclose(d);
                    }
                }
            }
        };
        auto next_video = [&] { return next_frame < frames.size() ? frames[next_frame].count : UINT64_MAX; };
        bus.set_counter(&env.count);
        core.reset();
        bus.take_init(core.m_reg);
        const auto t0 = std::chrono::steady_clock::now();
        while (!bus.finished()) {
            int vector;
            uint32_t pc;
            if (video) video_events();
            if (bus.take_irq(vector, pc)) {
                if (pc != core.m_reg[32] || !core.interrupts_enabled()) {
                    char b[128];
                    std::snprintf(b, sizeof b, "MAME took interrupt %d at PC %08x; ours is at %08x, PSW %08x", vector,
                                  pc, core.m_reg[32], core.psw());
                    throw m1::Divergence(b);
                }
                core.take_interrupt(vector);
                if (check) {
                    ref_bus.take_irq(vector, pc);
                    ref.take_interrupt(vector);
                }
            }
            if (check) {
                const uint32_t pc0 = ref.m_reg[32];
                v60::Interp::step(ref);
                ++ref_count;
                env.limit = env.count + 1;
                m1::run_native(core, env);
                for (int r = 0; r < 68; r++)
                    if (r != 32 + 1 && ref.m_reg[r] != core.m_reg[r]) {
                        char b[160];
                        std::snprintf(b, sizeof b,
                                      "after the instruction at %08x: r%d is %08x natively, %08x in the interpreter", pc0,
                                      r, core.m_reg[r], ref.m_reg[r]);
                        throw m1::Divergence(b);
                    }
                if (ref.psw() != core.psw()) {
                    char b[128];
                    std::snprintf(b, sizeof b, "after the instruction at %08x: PSW %08x natively, %08x in the interpreter",
                                  pc0, core.psw(), ref.psw());
                    throw m1::Divergence(b);
                }
                continue;
            }
            env.limit = std::min({bus.next_irq(), bus.end_count(), next_video()});
            if (env.limit == env.count) {
                if (video && next_video() == env.count) video_events();
                continue;
            }
            m1::run_native(core, env);
        }
        const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
#if M1_NATIVE_TGP
        if (!getenv("M1_REPLAY_TGP"))
            std::printf("TGP native: %" PRIu64 " instructions, %" PRIu64 " FIFO words identical to MAME; copro RAM "
                        "reads %" PRIu64 " (%" PRIu64 " busy polls MAME's TGP had not finished yet), %" PRIu64
                        " other differences%s%s\n",
                        copro.tgp_instructions(), bus.copro_fifo_words, bus.copro_ram_reads, bus.copro_ram_busy,
                        bus.copro_ram_mismatch,
                        bus.copro_ram_mismatch ? ", first: " : "", bus.first_ram_mismatch.c_str());
        if (timed)
            std::printf("TGP waits: MAME's V60 retried FIFO reads %" PRIu64 " times; ours waits the same at %" PRIu64
                        " reads, shorter at %" PRIu64 ", longer at %" PRIu64 " (%" PRIu64 " retries off)\n",
                        bus.wait_mame, bus.wait_same, bus.wait_short, bus.wait_long, bus.wait_off);
#endif
        if (video) {
            static const char *const names[3] = {"background tilemaps", "with the 3D", "composed screen"};
            std::printf("video native: %" PRIu64 " screen updates;", updates);
            for (int k = 0; k < 3; k++)
                std::printf(" %s %" PRIu64 " identical%s%s%s", names[k], same[k], first_diff[k].empty() ? "" : " (first differs: ",
                            first_diff[k].c_str(), first_diff[k].empty() ? "" : ")");
            std::printf("; display list control reads %" PRIu64 ", %" PRIu64 " differ%s%s\n", bus.listctl_reads,
                        bus.listctl_mismatch, bus.listctl_mismatch ? ", first: " : "", bus.first_listctl_mismatch.c_str());
        }
        if (!getenv("M1_REPLAY_IO"))
            std::printf("I/O board native: %" PRIu64 " reads (%" PRIu64 " polls of a command MAME's board was still "
                        "running), %" PRIu64 " other differences%s%s; %zu drive board commands\n",
                        bus.io_reads, bus.io_busy, bus.io_mismatch, bus.io_mismatch ? ", first: " : "",
                        bus.first_io_mismatch.c_str(), io.drive_commands.size());
#if M1_NATIVE_SOUND
        if (sound) {
            sound->write_wav(sound_path);
            std::printf("sound native: %zu bytes to the board, %" PRIu64 " 68000 instructions; audio in %s\n",
                        sound->board.bytes_received(), sound->board.instructions(), sound_path.c_str());
        }
#endif
        std::printf("m1native: MATCH. %" PRIu64 " instructions, all native (%" PRIu64 " recompiled), %" PRIu64
                    " device events, %" PRIu64 " interrupts, %" PRIu64 " samples identical to MAME; %.2f s (%.1f M instr/s)\n",
                    env.count, m1::native_instructions(), bus.events(), bus.irqs(), bus.samples(), s,
                    double(env.count) / s / 1e6);
        return 0;
    } catch (const m1::Divergence &d) {
        std::printf("m1native: DIVERGED after %" PRIu64 " instructions: %s\n", env.count, d.what());
        return 1;
    } catch (const std::exception &e) {
        std::printf("m1native: FATAL after %" PRIu64 " instructions: %s\n", env.count, e.what());
        return 1;
    }
}
