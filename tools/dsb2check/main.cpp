// dsb2check: the Digital Sound Board 2 (src/runtime/dsb2) held to MAME's
// (Top Skater). The bytes the i960 sent its UART (the m2trace plugin's
// M2TRACE_DSBLOG "T" lines) are fed to the board at MAME's times; the
// 68000's UART reads and its control and FIFO writes are compared with
// MAME's ("R" and "W" lines), and the board's output can be written for
// listening.
//
//   m2dsb2check ROM_CACHE_DIR MAME_DSB.log [--wav OUT.wav]
//
// Built with the recompiled 68000 program; reads the imported
// dsb_program.bin and dsb_mpeg.bin.
#include "runtime/dsb2.h"

#include <filesystem>
#include <iterator>
#include <set>

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
        std::fprintf(stderr, "usage: m2dsb2check ROM_CACHE_DIR MAME_DSB.log [--wav OUT.wav]\n");
        return 2;
    }
    std::string wav;
    for (int i = 3; i + 1 < argc; i += 2)
        if (!std::strcmp(argv[i], "--wav")) wav = argv[i + 1];
    try {
        std::vector<uint8_t> program = read_file(std::filesystem::path(argv[1]) / "dsb_program.bin");
        std::vector<uint8_t> mpeg = read_file(std::filesystem::path(argv[1]) / "dsb_mpeg.bin");
        snd::Dsb2 dsb(program, std::move(mpeg));
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
        std::printf("dsb2check: %zu MAME events, %zu of ours; %zu the same (worst time %.1f us)", want.size(), got.size(),
                    same, worst * 1e6);
        if (first != SIZE_MAX)
            std::printf("; first differs at event %zu (MAME %.6f %c %06x %02x)", first, want[first].t, want[first].kind,
                        want[first].port, want[first].value);
        const bool exact = same == want.size() && got.size() == want.size();
        // As dsbcheck: the UART's interrupt can put a read either side of an
        // unrelated write; the board does the same if each kind of event, per
        // address, has MAME's values in MAME's order.
        // Of the control port only bit 3, the MPEG ROM bank, reaches the
        // sound; the rest are lamps the program blinks as it goes.
        for (auto *v : {&want, &got})
            for (Event &e : *v)
                if (e.port == 0xd00001) e.value &= 0x08;
        std::set<std::pair<char, unsigned>> ports;
        for (const Event &e : want) ports.insert({e.kind, e.port});
        bool per_port = true;
        double worst_port = 0;
        for (const auto &[kind, port] : ports) {
            if (!per_port) break;
            std::vector<const Event *> a, b;
            // The control port is rewritten on every pass of the 68000's
            // loop, so only its changes count, and the program reads its
            // UART again until the interrupt has gone, so repeats of a byte
            // within 200 us are one read.
            auto pick = [&](const std::vector<Event> &from, std::vector<const Event *> &to) {
                for (const Event &e : from) {
                    if (e.kind != kind || e.port != port) continue;
                    if (!to.empty() && to.back()->value == e.value &&
                        (port == 0xd00001 || (kind == 'R' && e.t - to.back()->t < 200e-6)))
                        continue;
                    to.push_back(&e);
                }
            };
            pick(want, a), pick(got, b);
            while (b.size() > a.size() && b.back()->t > end - 0.001) b.pop_back(); // ours may run past MAME's log
            per_port = a.size() == b.size();
            for (size_t i = 0; per_port && i < a.size(); i++) {
                per_port = a[i]->value == b[i]->value;
                worst_port = std::fmax(worst_port, std::fabs(a[i]->t - b[i]->t));
            }
            if (!per_port) {
                size_t i = 0;
                while (i < a.size() && i < b.size() && a[i]->value == b[i]->value) i++;
                std::printf("\n%c %06x: MAME %zu, ours %zu; differ at %zu", kind, port, a.size(), b.size(), i);
                if (i < a.size()) std::printf(" (MAME %.6f %02x)", a[i]->t, a[i]->value);
                if (i < b.size()) std::printf(" (ours %.6f %02x)", b[i]->t, b[i]->value);
            }
        }
        if (exact) std::printf("\nMATCH\n");
        else if (per_port)
            std::printf("\nMATCH per port: every read and write has MAME's value in MAME's order (worst time "
                        "%.1f us); %zu events interleave differently\n",
                        worst_port * 1e6, want.size() - same);
        else std::printf("\nMISMATCH\n");
        if (!wav.empty()) write_wav(wav, audio, snd::Dsb2::kRate);
        return exact || per_port ? 0 : 1;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "dsb2check: %s\n", e.what());
        return 1;
    }
}
