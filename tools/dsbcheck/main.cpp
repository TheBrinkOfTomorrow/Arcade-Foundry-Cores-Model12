// dsbcheck: the Digital Sound Board (src/model1/dsb) held to MAME's. The
// bytes MAME's sound 68000 sent (scripts/dsb_tap.lua's "T" lines) are fed to
// the board at MAME's times; the Z80's reads and MPEG register writes are
// compared with MAME's ("R" and "W" lines), and the board's output can be
// written for listening.
//
//   dsbcheck ROMSET MAME_DSB.log [--set swa] [--wav OUT.wav]
//   dsbcheck ROM_CACHE_DIR MAME_DSB.log [--wav OUT.wav]
//
// The second form is a Model 2C game's (m2dsbcheck, built with its
// recompiled Z80 program): its imported dsb_program.bin and dsb_mpeg.bin, and
// the log of the m2trace plugin's M2TRACE_DSBLOG.
#include "model1/dsb.h"
#include "model1/rom_set.h"

#include <filesystem>
#include <iterator>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Event {
    double t;
    char kind; // T: the 68000 sent a byte; R: the Z80 read one; W: a register write
    unsigned port, value;
};

std::vector<Event> read_log(const std::string &path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot read " + path);
    std::vector<Event> out;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream s(line);
        Event e{};
        std::string port, value;
        if (s >> e.t >> e.kind >> port >> value) {
            e.port = unsigned(std::stoul(port, nullptr, 16)), e.value = unsigned(std::stoul(value, nullptr, 16));
            out.push_back(e);
        }
    }
    return out;
}

void write_wav(const std::string &path, const std::vector<float> &v, uint32_t rate) {
    std::vector<int16_t> pcm(v.size());
    for (size_t i = 0; i < v.size(); i++) pcm[i] = int16_t(std::lround(std::fmax(-1.0, std::fmin(1.0, v[i])) * 32767));
    std::ofstream o(path, std::ios::binary);
    auto u32 = [&](uint32_t x) { o.write(reinterpret_cast<const char *>(&x), 4); };
    auto u16 = [&](uint16_t x) { o.write(reinterpret_cast<const char *>(&x), 2); };
    const uint32_t bytes = uint32_t(pcm.size() * 2);
    o.write("RIFF", 4), u32(36 + bytes), o.write("WAVEfmt ", 8), u32(16), u16(1), u16(2), u32(rate), u32(rate * 4),
        u16(4), u16(16), o.write("data", 4), u32(bytes);
    o.write(reinterpret_cast<const char *>(pcm.data()), bytes);
}

std::vector<uint8_t> read_file(const std::filesystem::path &p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read " + p.string());
    return {std::istreambuf_iterator<char>(f), {}};
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: dsbcheck ROMSET MAME_DSB.log [--set swa] [--wav OUT.wav]\n");
        return 2;
    }
    std::string set_name = "swa", wav;
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--set")) set_name = argv[i + 1];
        else if (!std::strcmp(argv[i], "--wav")) wav = argv[i + 1];
    }
    try {
        std::vector<uint8_t> program, mpeg;
        if (std::filesystem::exists(std::filesystem::path(argv[1]) / "dsb_program.bin")) {
            program = read_file(std::filesystem::path(argv[1]) / "dsb_program.bin");
            mpeg = read_file(std::filesystem::path(argv[1]) / "dsb_mpeg.bin");
        } else {
            const m1::RomSet *set = m1::find_rom_set(set_name);
            if (!set) throw std::runtime_error("unknown set " + set_name);
            m1::Images img = m1::load_rom_set(*set, argv[1]);
            program = std::move(img.at("dsb_cpu")), mpeg = std::move(img.at("dsb_mpeg"));
        }
        m1::Dsb dsb(program, std::move(mpeg));
        const std::vector<Event> mame = read_log(argv[2]);
        std::vector<Event> want;
        double end = 0, line = 0;
        for (const Event &e : mame) {
            // The byte's ten bits on the 31,250 baud line, after the one before
            if (e.kind == 'T') line = std::fmax(line, e.t) + 10.0 / 31250, dsb.receive(uint8_t(e.value), line);
            else want.push_back(e);
            end = std::fmax(end, e.t);
        }
        const std::string ours_path = std::string(argv[2]) + ".ours";
        std::FILE *log = std::fopen(ours_path.c_str(), "w");
        dsb.set_log(log);
        std::vector<float> audio;
        for (double t = 0; t < end + 1; t += 0.01) {
            dsb.advance_to(t);
            const auto a = dsb.take();
            audio.insert(audio.end(), a.begin(), a.end());
        }
        std::fclose(log);
        std::vector<Event> got = read_log(ours_path);
        while (!got.empty() && got.back().t > end + 0.001) got.pop_back(); // past MAME's log
        size_t same = 0, first = SIZE_MAX;
        double worst = 0;
        for (size_t i = 0; i < std::min(want.size(), got.size()); i++) {
            if (want[i].kind == got[i].kind && want[i].port == got[i].port && want[i].value == got[i].value) {
                ++same;
                worst = std::fmax(worst, std::fabs(want[i].t - got[i].t));
            } else if (first == SIZE_MAX) {
                first = i;
            }
        }
        std::printf("dsbcheck: %zu MAME events, %zu of ours; %zu the same (worst time %.1f us)", want.size(), got.size(),
                    same, worst * 1e6);
        if (first != SIZE_MAX)
            std::printf("; first differs at event %zu (MAME %.6f %c %02x %02x)", first, want[first].t, want[first].kind,
                        want[first].port, want[first].value);
        const bool exact = same == want.size() && got.size() == want.size();
        // With the Z80 taking the UART's interrupt (Touring Car), an arrival a
        // few microseconds off MAME's (its i8251s step on a 2 us clock, the
        // bits on a 32 us one; our line is a plain 320 us per byte) can put a
        // read either side of an unrelated register write. What the board
        // does is then the same if each kind of event, per port, has MAME's
        // values in MAME's order.
        bool per_port = got.size() >= want.size();
        double worst_port = 0;
        for (const char kind : {'R', 'W'})
            for (unsigned port = 0; port < 0x100 && per_port; port++) {
                std::vector<const Event *> a, b;
                for (const Event &e : want)
                    if (e.kind == kind && e.port == port) a.push_back(&e);
                for (const Event &e : got)
                    if (e.kind == kind && e.port == port) b.push_back(&e);
                // ours may go on a little past where MAME's log was cut
                while (b.size() > a.size() && b.back()->t > end - 0.001) b.pop_back();
                per_port = a.size() == b.size();
                for (size_t i = 0; per_port && i < a.size(); i++) {
                    per_port = a[i]->value == b[i]->value;
                    worst_port = std::fmax(worst_port, std::fabs(a[i]->t - b[i]->t));
                }
            }
        if (exact) std::printf("\nMATCH\n");
        else if (per_port)
            std::printf("\nMATCH per port: every read and register write has MAME's value in MAME's order (worst time "
                        "%.1f us); %zu events interleave differently\n",
                        worst_port * 1e6, want.size() - same);
        else std::printf("\nMISMATCH\n");
        if (!wav.empty()) write_wav(wav, audio, m1::Dsb::kRate);
        return exact || per_port ? 0 : 1;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "dsbcheck: %s\n", e.what());
        return 1;
    }
}
