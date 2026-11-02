// m1io2check: the model1io2 I/O board (Wing War's) against MAME's dual-port
// RAM log (M1IO_LOG, a local logging build: each side's accesses with
// MAME's time and the CPU's cycle count). The V60's writes are replayed at
// their times; the Z80's accesses must be MAME's, at the same T-states, and
// the V60's reads must see what MAME's did. The V60's timeslice is taken to
// start on the microsecond grid or the board's last timer tick, from the
// instruction's start.
//
//   m1io2check BIOS.bin M1IO_LOG [--limit N]
#include "model1/io_board2.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iterator>
#include <string>

namespace {
struct Event {
    char side, rw;
    unsigned off, data;
    __int128 t;
    uint64_t cycles;
};
bool parse(const char *line, Event &e) {
    char side, rw;
    unsigned off, data, sec;
    long long as;
    unsigned long long cyc;
    if (std::sscanf(line, "%c%c %x %x %u.%lld %llu", &side, &rw, &off, &data, &sec, &as, &cyc) != 7) return false;
    e = {side, rw, off, data, (__int128)sec * 1000000000000000000 + as, cyc};
    return true;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: m1io2check BIOS.bin M1IO_LOG [--limit N]\n");
        return 2;
    }
    long limit = 20;
    for (int i = 3; i + 1 < argc; i++)
        if (!std::strcmp(argv[i], "--limit")) limit = std::atol(argv[i + 1]);
    std::ifstream bf(argv[1], std::ios::binary);
    std::vector<uint8_t> bios(std::istreambuf_iterator<char>(bf), {});
    m1::IoBoard2 io(bios, {});
    uint64_t now = 0;
    io.set_clock([&] { return now; });
    io.set_slices([&](uint64_t t) {
        const __int128 grid = (__int128)(t - t % 80) * m1::IoBoard2::kAttoPerTick;
        return std::max(grid, io.latest_cut((__int128)t * m1::IoBoard2::kAttoPerTick));
    });
    std::deque<Event> ours;
    io.on_left = [&](char rw, unsigned off, uint8_t v, uint64_t c) { ours.push_back({'L', rw, off, v, 0, c}); };
    FILE *f = std::fopen(argv[2], "r");
    if (!f) return 1;
    char line[256];
    long n = 0, bad = 0, v60reads = 0, v60bad = 0;
    std::deque<Event> mame;
    auto drain = [&] {
        while (!ours.empty() && !mame.empty()) {
            const Event a = ours.front(), b = mame.front();
            ours.pop_front(), mame.pop_front();
            ++n;
            if (a.rw != b.rw || a.off != b.off || a.data != b.data || a.cycles != b.cycles) {
                if (bad++ < limit)
                    std::printf("Z80 #%ld: ours %c %03x %02x at %llu, MAME's %c %03x %02x at %llu (%+lld)\n", n, a.rw, a.off,
                                a.data, (unsigned long long)a.cycles, b.rw, b.off, b.data,
                                (unsigned long long)b.cycles, (long long)(a.cycles - b.cycles));
            }
        }
    };
    try {
        while (std::fgets(line, sizeof line, f)) {
            Event e;
            if (!parse(line, e)) continue;
            if (e.side == 'L') {
                mame.push_back(e);
                drain();
                continue;
            }
            // MAME's time of a V60 access is its instruction's end (and a read's
            // after its wait state); its timeslice is the one it started in
            now = uint64_t(e.t / m1::IoBoard2::kAttoPerTick) - 40 - (e.rw == 'R' ? 5 : 0);
            if (e.rw == 'W') io.write(0xc00000 + e.off * 2, e.data, 2);
            else {
                const uint32_t v = io.read(0xc00000 + e.off * 2, 1);
                ++v60reads;
                if (v != e.data && v60bad++ < limit)
                    std::printf("V60 read %03x at %.9f: ours %02x, MAME's %02x\n", e.off, double(e.t) * 1e-18, v, e.data);
            }
            drain();
        }
        io.run_to((__int128)now * m1::IoBoard2::kAttoPerTick);
        drain();
    } catch (const std::exception &ex) {
        std::printf("stopped: %s (Z80 at %llu)\n", ex.what(), (unsigned long long)io.cycles());
    }
    std::printf("Z80 accesses: %ld compared, %ld differ (%zu of ours, %zu of MAME's left over); V60 reads: %ld, %ld differ\n", n,
                bad, ours.size(), mame.size(), v60reads, v60bad);
    return bad || v60bad ? 1 : 0;
}
