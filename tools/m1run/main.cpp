// m1run: the Model 1 game free-running on the native board (src/model1/
// board), as the core library runs it: no trace, no MAME. Headless: frames
// go to raw dumps (scripts/rgb2png.py converts them).
//
//   m1run ROMSET FRAMES [--set vr] [--inputs SCRIPT] [--dump DIR --every N]
//         [--frames MAME_FRAMES.txt] [--wav FILE] [--dsb-log FILE] [--io-log FILE]
//
// --inputs: a scripts/m1_inputs.lua controls script, by frame. --frames:
// MAME's frame log of a run with the same controls (patch 0006); each
// screen's hash is compared with MAME's for that frame (measured: the board
// runs on instruction counts, so the game can drift from MAME's timing).
// --wav: the sound boards' output, mixed to 48 kHz. --dsb-log: the Digital
// Sound Board's received bytes and MPEG register writes, timed (as
// scripts/dsb_tap.lua logs MAME's). --io-log: Wing War's I/O board, both
// sides of its dual-port RAM ("LR/LW/RR/RW byte data cycles", the Z80's in
// T-states, the V60's in 16 MHz cycles), as MAME's mb8421 logs them for
// m1io2check.
#include "../common/m1_inputs.h"
#include "app/link_socket.h"
#include "model1/board.h"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::map<uint64_t, uint64_t> read_mame_screens(const std::string &path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot read " + path);
    std::map<uint64_t, uint64_t> out;
    std::string line, k;
    uint64_t frame = 0;
    bool update = false;
    while (std::getline(f, line)) {
        std::istringstream s(line);
        s >> k;
        if (k == "su") s >> frame, update = true;
        else if (k == "vb") update = false;
        else if (k == "scr" && update) {
            std::string h;
            s >> h;
            out[frame] = std::stoull(h, nullptr, 16);
        }
    }
    return out;
}

// The boards' outputs, each at its own rate, mixed at equal gain to 48 kHz.
void write_wav(const std::string &path, const std::vector<std::pair<const std::vector<float> *, double>> &streams) {
    double seconds = 1e30;
    for (const auto &[v, rate] : streams) seconds = std::min(seconds, double(v->size() / 2) / rate);
    const size_t n = size_t(seconds * 48000);
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
            float sum = 0;
            for (const auto &[v, rate] : streams) sum += at(*v, t * rate, ch);
            out[2 * i + ch] = int16_t(std::clamp(sum, -1.0f, 1.0f) * 32767);
        }
    std::ofstream o(path, std::ios::binary);
    auto u32 = [&](uint32_t x) { o.write(reinterpret_cast<const char *>(&x), 4); };
    auto u16 = [&](uint16_t x) { o.write(reinterpret_cast<const char *>(&x), 2); };
    const uint32_t bytes = uint32_t(out.size() * 2);
    o.write("RIFF", 4), u32(36 + bytes), o.write("WAVEfmt ", 8), u32(16), u16(1), u16(2), u32(48000), u32(48000 * 4),
        u16(4), u16(16), o.write("data", 4), u32(bytes);
    o.write(reinterpret_cast<const char *>(out.data()), bytes);
}

bool load_file(const std::string &path, uint8_t *into, size_t size) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.read(reinterpret_cast<char *>(into), std::streamsize(size));
    return true;
}

void save_file(const std::string &path, const uint8_t *from, size_t size) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char *>(from), std::streamsize(size));
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: m1run ROMSET FRAMES [--set vr] [--inputs SCRIPT] [--dump DIR --every N] "
                             "[--frames MAME_FRAMES.txt] [--wav FILE] [--dsb-log FILE] [--io-log FILE] [--nvram DIR] [--save-nvram DIR] "
                             "[--link-listen PORT --link-next HOST:PORT [--link-sync]]\n");
        return 2;
    }
    std::string set_name = "vr", inputs_path, dump_dir, frames_path, wav_path, dsb_log_path, io_log_path, nvram_dir, save_nvram_dir, link_next;
    int every = 0, link_listen = 0;
    bool link_sync = false;
    const uint64_t frames = std::strtoull(argv[2], nullptr, 0);
    for (int i = 3; i < argc; i++)
        if (!std::strcmp(argv[i], "--link-sync")) link_sync = true;
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--link-sync")) {
            --i; // a flag
            continue;
        }
        if (!std::strcmp(argv[i], "--set")) set_name = argv[i + 1];
        else if (!std::strcmp(argv[i], "--inputs")) inputs_path = argv[i + 1];
        else if (!std::strcmp(argv[i], "--dump")) dump_dir = argv[i + 1];
        else if (!std::strcmp(argv[i], "--every")) every = std::atoi(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--frames")) frames_path = argv[i + 1];
        else if (!std::strcmp(argv[i], "--wav")) wav_path = argv[i + 1];
        else if (!std::strcmp(argv[i], "--dsb-log")) dsb_log_path = argv[i + 1];
        else if (!std::strcmp(argv[i], "--io-log")) io_log_path = argv[i + 1];
        else if (!std::strcmp(argv[i], "--nvram")) nvram_dir = argv[i + 1];
        else if (!std::strcmp(argv[i], "--save-nvram")) save_nvram_dir = argv[i + 1];
        else if (!std::strcmp(argv[i], "--link-listen")) link_listen = std::atoi(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--link-next")) link_next = argv[i + 1];
    }
    try {
        const m1::RomSet *set = m1::find_rom_set(set_name);
        if (!set) throw std::runtime_error("unknown set " + set_name);
        m1::Board board(m1::load_rom_set(*set, argv[1]), *set);
        // The core's save files (src/core/m1core.cpp): the I/O board's EEPROM, the backup RAM
        if (!nvram_dir.empty()) {
            if (load_file(nvram_dir + "/ioboard_eeprom.bin", board.eeprom().data(), board.eeprom().size()))
                board.reload_settings();
            load_file(nvram_dir + "/backup_ram.bin", board.backup_ram(), m1::Board::kBackupBytes);
        }
        std::unique_ptr<app::TcpLink> link;
        if (link_listen > 0 || !link_next.empty()) {
            link = std::make_unique<app::TcpLink>(uint16_t(link_listen), link_next);
            if (!link->error().empty()) throw std::runtime_error("link: " + link->error());
            board.set_link(link.get(), link_sync);
            // Both cables up before the first frame, as two MAMEs started
            // together have them by the time the game enables its board
            const auto give_up = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (!(link->rx_open() && link->tx_open()) && std::chrono::steady_clock::now() < give_up) {
                link->poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        const auto script = inputs_path.empty() ? std::vector<m1tools::InputLine>{} : m1tools::read_inputs(inputs_path);
        const auto mame = frames_path.empty() ? std::map<uint64_t, uint64_t>{} : read_mame_screens(frames_path);
        uint64_t same = 0, compared = 0, first_diff = UINT64_MAX, last_same = UINT64_MAX, near = 0;
        int64_t worst = 0;
        // A screen of ours MAME showed within a few frames: the game's timing
        // drifted, its picture did not.
        std::multimap<uint64_t, uint64_t> by_hash;
        for (auto &[f, h] : mame) by_hash.emplace(h, f);
        std::vector<float> fm, pcm, mpeg;
        std::unique_ptr<std::FILE, int (*)(std::FILE *)> dsb_log(nullptr, std::fclose);
        if (!dsb_log_path.empty() && board.dsb()) {
            dsb_log.reset(std::fopen(dsb_log_path.c_str(), "w"));
            board.dsb()->set_log(dsb_log.get());
        }
        std::unique_ptr<std::FILE, int (*)(std::FILE *)> io_log(nullptr, std::fclose);
        if (!io_log_path.empty() && board.io2()) {
            io_log.reset(std::fopen(io_log_path.c_str(), "w"));
            std::FILE *o = io_log.get();
            board.io2()->on_left = [o](char rw, unsigned i, uint8_t v, uint64_t c) {
                std::fprintf(o, "L%c %03x %02x %" PRIu64 "\n", rw, i, v, c);
            };
            board.io2()->on_right = [o](char rw, unsigned i, uint8_t v, uint64_t t) {
                std::fprintf(o, "R%c %03x %02x %" PRIu64 "\n", rw, i, v, t / m1::Copro::kTicksPerV60Cycle);
            };
        }
        const auto t0 = std::chrono::steady_clock::now();
        for (uint64_t f = 0; f < frames; f++) {
            board.controls() = m1tools::controls_at(script, f, set_name);
            board.run_frame();
            if (getenv("M1RUN_TRACE_FRAMES"))
                std::printf("frame %" PRIu64 ": %" PRIu64 " instructions, waited %" PRIu64 " ticks, screen %016" PRIx64 ", pc %06x\n", f,
                            board.instructions(), board.stall_ticks(), m1::Video::hash(board.screen()), board.pc());
            if (!wav_path.empty()) {
                const auto a = board.sound().take_fm(), b = board.sound().take_pcm();
                fm.insert(fm.end(), a.begin(), a.end());
                pcm.insert(pcm.end(), b.begin(), b.end());
                if (board.dsb()) {
                    const auto c = board.dsb()->take();
                    mpeg.insert(mpeg.end(), c.begin(), c.end());
                }
            } else {
                board.sound().take_fm(), board.sound().take_pcm();
                if (board.dsb()) board.dsb()->take();
            }
            if (auto it = mame.find(f); it != mame.end()) {
                ++compared;
                const uint64_t h = m1::Video::hash(board.screen());
                if (h == it->second) ++same, last_same = f;
                else if (first_diff == UINT64_MAX) first_diff = f;
                int64_t best = INT64_MAX;
                for (auto [a, b] = by_hash.equal_range(h); a != b; ++a)
                    if (std::abs(int64_t(a->second) - int64_t(f)) < std::abs(best)) best = int64_t(a->second) - int64_t(f);
                if (best != INT64_MAX && std::abs(best) <= 8) ++near, worst = std::max(worst, int64_t(std::abs(best)));
            }
            if (!dump_dir.empty() && every > 0 && f % uint64_t(every) == 0) {
                char path[512];
                std::snprintf(path, sizeof path, "%s/m1run_%05u.rgb", dump_dir.c_str(), unsigned(f));
                if (FILE *d = std::fopen(path, "wb")) {
                    std::fwrite(board.screen().data(), 4, board.screen().size(), d);
                    std::fclose(d);
                }
            }
        }
        const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (!save_nvram_dir.empty()) {
            save_file(save_nvram_dir + "/ioboard_eeprom.bin", board.eeprom().data(), board.eeprom().size());
            save_file(save_nvram_dir + "/backup_ram.bin", board.backup_ram(), m1::Board::kBackupBytes);
        }
        if (!wav_path.empty()) {
            std::vector<std::pair<const std::vector<float> *, double>> streams = {{&fm, board.sound().fm_rate()},
                                                                                {&pcm, board.sound().pcm_rate()}};
            if (board.dsb()) streams.emplace_back(&mpeg, board.dsb()->rate());
            write_wav(wav_path, streams);
        }
        std::printf("m1run: %" PRIu64 " frames in %.2f s (%.0f fps), last screen hash %016" PRIx64 "\n", frames, s,
                    double(frames) / s, m1::Video::hash(board.screen()));
        if (board.interpreted())
            std::printf("m1run: fallback interpreter ran %" PRIu64 " V60 instructions, entered at %zu addresses\n",
                        board.interpreted(), board.fallback_entries());
        if (compared) {
            std::printf("m1run: %" PRIu64 " of %" PRIu64 " screens identical to MAME's", same, compared);
            if (first_diff != UINT64_MAX) std::printf("; first differs at frame %" PRIu64 ", last identical %" PRIu64, first_diff, last_same);
            std::printf("\nm1run: %" PRIu64 " screens are MAME's within %" PRId64 " frames\n", near, worst);
        }
        if (link) {
            static const char *const states[] = {"off", "waiting", "up", "lost"};
            const m1::CommBoard &cb = board.comm_board();
            std::printf("m1run: link %s", states[int(cb.link())]);
            if (cb.link() == m1::CommBoard::Link::Up) std::printf(", cabinet %d of %d", cb.id(), cb.count());
            std::printf("\n");
        }
        return 0;
    } catch (const std::exception &e) {
        std::printf("m1run: FATAL: %s\n", e.what());
        return 1;
    }
}
