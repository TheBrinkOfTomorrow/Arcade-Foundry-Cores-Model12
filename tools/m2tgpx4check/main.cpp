// m2tgpx4check: lockstep check of the recompiled TGPx4 program against MAME.
//
//   m2tgpx4check TGPX4.log
//
// TGPX4.log is MAME's TGP-side event log on a Model 2C game (M2TRACE_TGPLOG,
// patches/mame/0009): the program upload ("xup"), then every word the TGPx4
// popped from its input FIFO and pushed to its output FIFO, and its buffer
// RAM and copro_data accesses, in order. The recompiled program boots from
// the upload and runs against the log with one cursor: each input pop gets
// MAME's next "in" word (the FIFO is empty, and the pop stalls, when MAME's
// next event is not one), each buffer RAM or copro_data read MAME's value,
// and every other event must be MAME's next one, bit for bit.

#include "runtime/tgpx4/tgpx4_gen.h"

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Divergence : std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct EndOfLog {}; // our TGPx4 went on past where MAME's run stopped

struct Ev {
    enum Kind { In, Out, Mr, Mw, Cr } kind;
    uint32_t a = 0, b = 0;
};
const char *kname[] = {"in", "out", "mr", "mw", "cr"};

class LogBus : public rt::tgpx4::Tgpx4Bus {
public:
    std::vector<Ev> ev;
    size_t i = 0, clears = 0, unmapped = 0;
    rt::tgpx4::Tgpx4 *t = nullptr;

    std::string next() const {
        if (i >= ev.size()) return "end of MAME's log";
        char b[96];
        std::snprintf(b, sizeof b, "MAME's event %zu is \"%s %x %x\"", i, kname[ev[i].kind], ev[i].a, ev[i].b);
        return b;
    }
    [[noreturn]] void diverge(const std::string &what) {
        if (i >= ev.size()) throw EndOfLog{};
        char b[96];
        std::snprintf(b, sizeof b, " (TGPx4 instruction %" PRIu64 ", pc %03x)", t->count, t->m_core->ppc);
        throw Divergence(what + "; " + next() + b);
    }
    bool at(Ev::Kind k) const { return i < ev.size() && ev[i].kind == k; }

    bool in_empty() override { return !at(Ev::In); }
    bool in_full() override { return false; }
    uint32_t in_pop() override { return ev[i++].a; }
    void in_clear() override { ++clears; } // MAME's log has no clears: the FIFOs are taken as empty then
    bool out_empty() override { return true; }
    bool out_full() override { return false; }
    void out_push(uint32_t v) override {
        char b[64];
        if (at(Ev::Out) && ev[i].a == v) {
            ++i;
            return;
        }
        std::snprintf(b, sizeof b, "our TGPx4 output %08x", v);
        diverge(b);
    }
    void out_clear() override { ++clears; }
    uint32_t ext_r(uint32_t a) override {
        char b[64];
        if (a >= 0x400000 && a < 0x800000) {
            if (at(Ev::Mr) && ev[i].a == (a & 0x7fff)) return ev[i++].b;
            std::snprintf(b, sizeof b, "our TGPx4 read buffer RAM %04x", a & 0x7fff);
            diverge(b);
        }
        if (a >= 0x800000 && a < 0xa00000) {
            if (at(Ev::Cr) && ev[i].a == a - 0x800000) return ev[i++].b;
            std::snprintf(b, sizeof b, "our TGPx4 read copro_data %06x", a - 0x800000);
            diverge(b);
        }
        ++unmapped;
        return 0; // MAME's unmapped reads
    }
    void ext_w(uint32_t a, uint32_t v) override {
        char b[80];
        if (a >= 0x400000 && a < 0x800000) {
            if (at(Ev::Mw) && ev[i].a == (a & 0x7fff) && ev[i].b == v) {
                ++i;
                return;
            }
            std::snprintf(b, sizeof b, "our TGPx4 wrote %08x to buffer RAM %04x", v, a & 0x7fff);
            diverge(b);
        }
        ++unmapped; // MAME's unmapped writes do nothing
    }
};

uint32_t crc32_words(const std::vector<uint32_t> &w) {
    uint32_t c = 0xffffffffu;
    for (uint32_t v : w)
        for (int k = 0; k < 4; k++) {
            c ^= (v >> (8 * k)) & 0xff;
            for (int j = 0; j < 8; j++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
        }
    return ~c;
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: m2tgpx4check TGPX4.log\n");
        return 2;
    }
    LogBus bus;
    rt::tgpx4::Tgpx4 t;
    bus.t = &t;
    t.bus = &bus;
    try {
        std::ifstream lf(argv[1]);
        if (!lf) throw std::runtime_error(std::string("cannot open ") + argv[1]);
        std::vector<uint32_t> upload;
        std::string line;
        while (std::getline(lf, line)) {
            std::istringstream ls(line);
            std::string k;
            ls >> k >> std::hex;
            Ev e{};
            if (k == "xup") {
                if (!upload.empty()) throw std::runtime_error("more than one TGPx4 upload in the log");
                unsigned n = 0;
                ls >> std::dec >> n >> std::hex;
                upload.resize(n);
                for (auto &w : upload) ls >> w;
                continue;
            }
            if (k == "in") e.kind = Ev::In, ls >> e.a;
            else if (k == "out") e.kind = Ev::Out, ls >> e.a;
            else if (k == "mr") e.kind = Ev::Mr, ls >> e.a >> e.b;
            else if (k == "mw") e.kind = Ev::Mw, ls >> e.a >> e.b;
            else if (k == "cr") e.kind = Ev::Cr, ls >> e.a >> e.b;
            else continue;
            bus.ev.push_back(e);
        }
        if (upload.size() != rt::tgpx4gen::upload_words || crc32_words(upload) != rt::tgpx4gen::upload_crc32)
            throw Divergence("the i960 uploaded a TGPx4 program other than the one recompiled");

        // MAME machine_reset: the TGPx4 is held from reset until the upload
        // ends (copro_boot clears the halt).
        t.reset();
        const auto t0 = std::chrono::steady_clock::now();
        try {
            while (bus.i < bus.ev.size()) {
                const size_t before = bus.i;
                rt::tgpx4gen::run(t, UINT64_MAX);
                if (bus.i == before) bus.diverge("our TGPx4 stalls on its empty input FIFO");
            }
        } catch (const EndOfLog &) {
        }
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        size_t n[5] = {};
        for (auto &e : bus.ev) n[e.kind]++;
        std::printf("m2tgpx4check: MATCH. %" PRIu64 " TGPx4 instructions, all native (%u-instruction program); %zu input "
                    "words, %zu output words, %zu buffer RAM reads, %zu writes, %zu copro_data reads identical to MAME; "
                    "%.2f s\n",
                    t.count, rt::tgpx4gen::program_size, n[Ev::In], n[Ev::Out], n[Ev::Mr], n[Ev::Mw], n[Ev::Cr], secs);
        return 0;
    } catch (const Divergence &d) {
        std::printf("m2tgpx4check: DIVERGED: %s\n", d.what());
        return 1;
    } catch (const rt::tgpx4::Tgpx4Fatal &f) {
        std::printf("m2tgpx4check: FATAL after %" PRIu64 " TGPx4 instructions (pc %03x): %s; %s\n", t.count, t.m_core->ppc,
                    f.what(), bus.next().c_str());
        return 1;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "m2tgpx4check: %s\n", e.what());
        return 2;
    }
}
