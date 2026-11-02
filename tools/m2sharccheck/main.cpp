// m2sharccheck: lockstep check of the recompiled SHARC program against MAME.
//
//   m2sharccheck SHARC.log
//
// SHARC.log is MAME's TGP-side event log on a Model 2B game (M2TRACE_TGPLOG,
// patches/mame/0008): the host boot upload ("sup"), then every word the
// SHARC popped from its input FIFO and pushed to its output FIFO, and its
// buffer RAM and copro_data accesses, in order. The recompiled program boots
// from the upload and runs against the log with one cursor: each input pop
// gets MAME's next "in" word, each buffer RAM or copro_data read MAME's
// value, and every other event must be MAME's next one, bit for bit. The
// input FIFO's empty flag (FLAG0) is clear exactly when MAME's next event
// is an input pop.

#include "runtime/sharc/sharc_gen.h"

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

struct Ev {
    enum Kind { In, Out, Mr, Mw, Cr } kind;
    uint32_t a = 0, b = 0;
};
const char *kname[] = {"in", "out", "mr", "mw", "cr"};

class LogBus : public rt::sharc::SharcBus {
public:
    std::vector<Ev> ev;
    size_t i = 0;
    rt::sharc::Sharc *s = nullptr;

    std::string next() const {
        if (i >= ev.size()) return "end of MAME's log";
        char b[96];
        std::snprintf(b, sizeof b, "MAME's event %zu is \"%s %x %x\"", i, kname[ev[i].kind], ev[i].a, ev[i].b);
        return b;
    }
    [[noreturn]] void diverge(const std::string &what) {
        char b[96];
        std::snprintf(b, sizeof b, " (SHARC instruction %" PRIu64 ", pc %05x)", s->count, s->m_core->pc);
        throw Divergence(what + "; " + next() + b);
    }
    void step() {
        ++i;
        s->m_core->flag[0] = !(i < ev.size() && ev[i].kind == Ev::In);
    }
    uint32_t ext_r(uint32_t a) override {
        char b[64];
        if (a >= 0x400000 && a < 0xc00000) {
            if (i < ev.size() && ev[i].kind == Ev::In) {
                const uint32_t v = ev[i].a;
                step();
                return v;
            }
            diverge("our SHARC read its input FIFO");
        }
        if (a >= 0x1400000 && a < 0x1c00000) {
            if (i < ev.size() && ev[i].kind == Ev::Mr && ev[i].a == (a & 0x7fff)) {
                const uint32_t v = ev[i].b;
                step();
                return v;
            }
            std::snprintf(b, sizeof b, "our SHARC read buffer RAM %04x", a & 0x7fff);
            diverge(b);
        }
        if (a >= 0x1c00000 && a < 0x1e00000) {
            if (i < ev.size() && ev[i].kind == Ev::Cr && ev[i].a == a - 0x1c00000) {
                const uint32_t v = ev[i].b;
                step();
                return v;
            }
            std::snprintf(b, sizeof b, "our SHARC read copro_data %06x", a - 0x1c00000);
            diverge(b);
        }
        std::snprintf(b, sizeof b, "our SHARC read unmapped %08x", a);
        diverge(b);
    }
    void ext_w(uint32_t a, uint32_t v) override {
        char b[80];
        if (a >= 0xc00000 && a < 0x1400000) {
            if (i < ev.size() && ev[i].kind == Ev::Out && ev[i].a == v) return step();
            std::snprintf(b, sizeof b, "our SHARC output %08x", v);
            diverge(b);
        }
        if (a >= 0x1400000 && a < 0x1c00000) {
            if (i < ev.size() && ev[i].kind == Ev::Mw && ev[i].a == (a & 0x7fff) && ev[i].b == v) return step();
            std::snprintf(b, sizeof b, "our SHARC wrote %08x to buffer RAM %04x", v, a & 0x7fff);
            diverge(b);
        }
        std::snprintf(b, sizeof b, "our SHARC wrote %08x to unmapped %08x", v, a);
        diverge(b);
    }
};

uint32_t crc32_u16(const std::vector<uint16_t> &w) {
    uint32_t c = 0xffffffffu;
    for (uint16_t v : w)
        for (int k = 0; k < 2; k++) {
            c ^= (v >> (8 * k)) & 0xff;
            for (int j = 0; j < 8; j++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
        }
    return ~c;
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: m2sharccheck SHARC.log\n");
        return 2;
    }
    LogBus bus;
    rt::sharc::Sharc s;
    bus.s = &s;
    s.bus = &bus;
    try {
        std::ifstream lf(argv[1]);
        if (!lf) throw std::runtime_error(std::string("cannot open ") + argv[1]);
        std::vector<uint16_t> upload;
        std::string line;
        while (std::getline(lf, line)) {
            std::istringstream ls(line);
            std::string k;
            ls >> k >> std::hex;
            Ev e{};
            if (k == "sup") {
                if (!upload.empty()) throw std::runtime_error("more than one SHARC upload in the log");
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
        if (upload.size() != rt::sharcgen::upload_words || crc32_u16(upload) != rt::sharcgen::upload_crc32)
            throw Divergence("the i960 uploaded a SHARC program other than the one recompiled");

        // MAME machine_reset: the SHARC is reset and held, FLAG0 (input FIFO
        // empty) set, FLAG1 clear; the i960 boots it through the host port.
        s.reset();
        s.set_flag_input(0, 1);
        s.set_flag_input(1, 0);
        for (size_t k = 0; k < upload.size(); k++) s.external_dma_write(uint32_t(k), upload[k]);
        s.m_core->flag[0] = !(!bus.ev.empty() && bus.ev[0].kind == Ev::In);

        const auto t0 = std::chrono::steady_clock::now();
        while (bus.i < bus.ev.size()) {
            const auto stop = rt::sharcgen::run(s, UINT64_MAX);
            if (stop == rt::sharcgen::Stop::Idle) bus.diverge("our SHARC went idle");
            if (bus.i < bus.ev.size()) bus.diverge("our SHARC waits on its FIFO flags");
        }
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        size_t n[5] = {};
        for (auto &e : bus.ev) n[e.kind]++;
        std::printf("m2sharccheck: MATCH. %" PRIu64 " SHARC instructions, all native (%u-instruction program); %zu input "
                    "words, %zu output words, %zu buffer RAM reads, %zu writes, %zu copro_data reads identical to MAME; "
                    "%.2f s\n",
                    s.count, rt::sharcgen::program_size, n[Ev::In], n[Ev::Out], n[Ev::Mr], n[Ev::Mw], n[Ev::Cr], secs);
        return 0;
    } catch (const Divergence &d) {
        std::printf("m2sharccheck: DIVERGED: %s\n", d.what());
        return 1;
    } catch (const rt::sharc::SharcFatal &f) {
        std::printf("m2sharccheck: FATAL after %" PRIu64 " SHARC instructions (pc %05x): %s; %s\n", s.count, s.m_core->pc, f.what(),
                    bus.next().c_str());
        return 1;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "m2sharccheck: %s\n", e.what());
        return 2;
    }
}
