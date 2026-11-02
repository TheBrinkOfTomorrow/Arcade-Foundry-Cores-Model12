// m2native: the M1 lockstep harness for recompiled code. Same checks as
// m2replay (every device access, every sample's RAM hashes and registers,
// every interrupt against MAME), but the i960 code is the native C++ that
// m2recomp generated. There is no fallback: control reaching an address with
// no recompiled code is a hard error naming it.
//
//   m2native IMAGES_DIR TRACE.m2tr IRQ.log
//
// M2NATIVE_TGPLOG=TGP.log (MAME's TGP log of the same run): the TGP's reads
// of buffer RAM are measured, MAME's taken. The i960 writes buffer RAM
// (clears, geometrizer pushes) while MAME's TGP is still working on earlier
// input; what the TGP reads there depends on how far it had run.

#include "runtime/gen_support.h"
#include "runtime/lockstep.h"
#include "runtime/m2_replay_bus.h"
#include "runtime/m2_board.h"
#include "runtime/m2_tgp_board.h"
#include "geocheck.h"

#include <memory>

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
// The 315-5881 security chip (rt::kCrypt5881 games): native, strict, in
// front of the next model. It reads its stream from the bus's RAM at
// 0x01d80000 as MAME's crypt_read_callback does.
class CryptModel : public rt::DeviceModel {
public:
    CryptModel(rt::DeviceModel &next, rt::M2ReplayBus &bus)
        : next_(next), crypt_(rt::kCrypt5881Key, [&bus](uint32_t a) {
              const uint32_t o = (2 * a) & 0xfffe;
              const uint16_t w = uint16_t(bus.peek(0x01d80000 + (o & ~3u)) >> ((o & 2) * 8));
              return uint16_t(w >> 8 | w << 8);
          }) {}
    bool claims(uint32_t a) const override { return mine(a) || next_.claims(a); }
    Check read(uint32_t a, uint32_t mask, uint32_t &v) override {
        if (!mine(a)) return next_.read(a, mask, v);
        const uint32_t o = a & 0xfffc;
        v = ((mask & 0x0000ffff) ? uint32_t(crypt_.read16(o)) : 0) | ((mask & 0xffff0000) ? uint32_t(crypt_.read16(o + 2)) << 16 : 0);
        ++reads;
        return Strict;
    }
    void write(uint32_t a, uint32_t data, uint32_t mask) override {
        if (!mine(a)) return next_.write(a, data, mask);
        const uint32_t o = a & 0xfffc;
        if (mask & 0x0000ffff) crypt_.write16(o, uint16_t(data), uint16_t(mask));
        if (mask & 0xffff0000) crypt_.write16(o + 2, uint16_t(data >> 16), uint16_t(mask >> 16));
    }
    void measured_mismatch(uint32_t a, uint32_t ours, uint32_t mame) override { next_.measured_mismatch(a, ours, mame); }
    const uint8_t *region(uint32_t base) const override { return next_.region(base); }
    uint64_t reads = 0;

private:
    static bool mine(uint32_t a) { return a >= 0x01d90000 && a <= 0x01d9ffff; }
    rt::DeviceModel &next_;
    rt::Crypt5881 crypt_;
};

// Model 2A-CRX: the 315-5649 I/O chip and the EEPROM modelled natively
// (strict: every read must equal MAME's), in front of the TGP board model.
// The controls are MAME's: bits that come from them are measured (MAME's
// value used), so a run with inputs replayed still checks everything else.
class IoModel : public rt::DeviceModel {
public:
    explicit IoModel(rt::DeviceModel &next) : next_(next) {}
    void set_clock(const rt::Lockstep &ls) { io_.set_clock([&ls] { return rt::ticks_for_instructions(ls.count); }); }
    bool claims(uint32_t a) const override { return mine(a) || next_.claims(a); }
    Check read(uint32_t a, uint32_t mask, uint32_t &v) override {
        if (!mine(a)) return next_.read(a, mask, v);
        const uint32_t i = ((a & 0x1f) >> 2) * 2;
        inputs_ = ((mask & 0x000000ff) ? uint32_t(io_.input_bits(i)) : 0) |
                  ((mask & 0x00ff0000) ? uint32_t(io_.input_bits(i + 1)) << 16 : 0);
        v = ((mask & 0x000000ff) ? uint32_t(io_.read(i)) : 0) | ((mask & 0x00ff0000) ? uint32_t(io_.read(i + 1)) << 16 : 0);
        if (dbg_) std::fprintf(stderr, "R %08x %08x %08x\n", a, mask, v);
        // port B carries the EEPROM's DO: how long a write keeps it busy
        // depends on time, which we only estimate (measured; other bits strict)
        do_ = a == 0x01c00000 && (mask & 0x00ff0000) ? 0x00200000u : 0;
        return do_ || inputs_ ? Measured : Strict;
    }
    void write(uint32_t a, uint32_t data, uint32_t mask) override {
        if (!mine(a)) return next_.write(a, data, mask);
        const uint32_t i = ((a & 0x1f) >> 2) * 2;
        if (dbg_) std::fprintf(stderr, "W %08x %08x %08x\n", a, mask, data & mask);
        if (mask & 0x000000ff) io_.write(i, uint8_t(data));
        if (mask & 0x00ff0000) io_.write(i + 1, uint8_t(data >> 16));
    }
    void measured_mismatch(uint32_t a, uint32_t ours, uint32_t mame) override {
        if (!mine(a)) return next_.measured_mismatch(a, ours, mame);
        if ((ours ^ mame) & ~(do_ | inputs_)) {
            char b[96];
            std::snprintf(b, sizeof b, "I/O read %08x: ours %08x, MAME %08x", a, ours, mame);
            throw rt::Divergence(b);
        }
        ++((ours ^ mame) & do_ ? do_mismatch : input_reads);
    }
    uint64_t do_mismatch = 0; // EEPROM busy reads that differ
    uint64_t input_reads = 0; // reads whose controls differ (MAME's taken)
    const uint8_t *region(uint32_t base) const override { return next_.region(base); }

private:
    static bool mine(uint32_t a) { return a >= 0x01c00000 && a <= 0x01c0001f; }
    rt::DeviceModel &next_;
    uint32_t do_ = 0, inputs_ = 0; // the last read's measured bits
    bool dbg_ = std::getenv("M2_NATIVE_IO_LOG") != nullptr;
    rt::IoBoard board_;
    rt::Io5649 io_{board_};
};

// M2NATIVE_TGPLOG: MAME's "mr A V" lines in order; the TGP makes the same
// reads in the same order (m2tgpcheck), so the n-th read is MAME's n-th.
struct TgpLogOracle : rt::TgpBoard::MemOracle {
    explicit TgpLogOracle(const char *path) : f(path) {
        if (!f) throw std::runtime_error(std::string("cannot open ") + path);
    }
    uint32_t mem_r(uint32_t adr, uint32_t ours) override {
        std::string l;
        while (std::getline(f, l))
            if (l.compare(0, 3, "mr ") == 0) {
                ++reads;
                const uint32_t mame = uint32_t(std::stoul(l.substr(l.find(' ', 3) + 1), nullptr, 16));
                if (mame != ours && !mismatch++) {
                    char b[96];
                    std::snprintf(b, sizeof b, "%06x: ours %08x, MAME %08x (read %" PRIu64 ")", adr, ours, mame, reads);
                    first = b;
                }
                return mame;
            }
        throw rt::Divergence("the TGP read memory past the end of MAME's TGP log");
    }
    std::ifstream f;
    uint64_t reads = 0, mismatch = 0;
    std::string first;
};

std::vector<uint8_t> load(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 4 && argc != 5) {
        std::fprintf(stderr, "usage: m2native IMAGES_DIR TRACE.m2tr IRQ.log [GEO.log]\n");
        return 2;
    }
    const std::string dir = argv[1];
    uint64_t done = 0;
    try {
        rt::M2ReplayBus bus(load(dir + "/program.bin"), load(dir + "/main_data.bin"), argv[2]);
        rt::Cpu core(&bus);
        bus.attach(&core);
        // M2: the geometry ports, TGP and buffer RAM are native, not replayed
        // (Model 2A-CRX's are model2_tgp_mem's too).
        rt::TgpBoard board(load(dir + "/copro_tables.bin"), load(dir + "/copro_data.bin"));
        rt::TgpBoardModel geo(board);
        std::unique_ptr<TgpLogOracle> tgplog;
        if (const char *p = std::getenv("M2NATIVE_TGPLOG")) board.oracle = (tgplog = std::make_unique<TgpLogOracle>(p)).get();
        // M2_NATIVE_IO=1 (Model 2A): the I/O chip native too
        IoModel io(geo);
        rt::DeviceModel *model = rt::kModel2a && std::getenv("M2_NATIVE_IO") ? static_cast<rt::DeviceModel *>(&io) : &geo;
        CryptModel crypt(*model, bus);
        if (rt::kCrypt5881) model = &crypt;
        bus.set_model(model);
        rt::Lockstep ls(core, argv[3]);
        io.set_clock(ls);
        core.reset();
        gen::Env env{core, ls};
        // M2: the geometrizer, checked at each vblank against MAME's GEO.log.
        std::unique_ptr<rt::Geo> geom;
        std::unique_ptr<GeoCheck> geocheck;
        if (argc == 5) {
            geom = std::make_unique<rt::Geo>(load(dir + "/polygons.bin"), load(dir + "/textures.bin"), board.buffer_data());
            geocheck = std::make_unique<GeoCheck>(argv[4], *geom, board, bus, ls);
        }

        const auto t0 = std::chrono::steady_clock::now();
        while (!ls.finished() && !bus.trace_done()) {
            if (!gen::has_code(core.m_IP)) {
                char b[128];
                std::snprintf(b, sizeof b, "no recompiled code at %08x (instruction %" PRIu64 "): add it to the seeds",
                              core.m_IP, ls.count);
                throw rt::Fatal(b);
            }
            gen::run(env);
            done = ls.count;
        }
        done = ls.count;
        if (!bus.all_events_consumed())
            throw rt::Divergence("MAME's run ended but the trace still has events we did not make; " + bus.where());
        const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("m2native: MATCH. %" PRIu64 " instructions, all native (%" PRIu64 " recompiled), %" PRIu64
                    " epochs, %" PRIu64 " device events, %d interrupts, identical to MAME; %.2f s (%.1f M instr/s)\n",
                    ls.count, gen::native_instructions(), bus.epochs_matched(), bus.events_matched(), ls.interrupts(), s,
                    double(ls.count) / s / 1e6);
        std::printf("  TGP (recompiled, run on demand): %" PRIu64 " instructions, %" PRIu64
                    " output words identical to MAME\n"
                    "  measured (depends on relative timing): buffer RAM reads %" PRIu64 " differ of %" PRIu64
                    "; FIFO-status polls %" PRIu64 " differ of %" PRIu64 "; buffer RAM hash differs at %" PRIu64 " of %" PRIu64 " samples\n",
                    board.tgp_instructions(), geo.fifo_words, geo.buffer_mismatch, geo.buffer_reads, geo.status_mismatch,
                    geo.status_reads, geo.region_mismatch, geo.region_samples);
        if (geocheck)
            std::printf("  geometrizer (native): %" PRIu64 " frames, %" PRIu64 " rasterizer words and %" PRIu64
                        " polygons identical to MAME\n",
                        geocheck->frames_checked, geocheck->words, geocheck->polys);
        if (geocheck) {
            std::printf("  3D layer (CPU reference rasterizer): %" PRIu64 " of %" PRIu64 " frames identical to MAME%s%s\n",
                        geocheck->fb_checked - geocheck->fb_mismatch, geocheck->fb_checked,
                        geocheck->fb_mismatch ? "; first difference " : "", geocheck->first_fb_mismatch.c_str());
            std::printf("  screen (tilemaps + 3D, composed): %" PRIu64 " of %" PRIu64 " frames identical to MAME%s%s\n",
                        geocheck->scr_checked - geocheck->scr_mismatch, geocheck->scr_checked,
                        geocheck->scr_mismatch ? "; first difference " : "", geocheck->first_scr_mismatch.c_str());
        }
        if (rt::kModel2a && std::getenv("M2_NATIVE_IO"))
            std::printf("  I/O chip and EEPROM (native): strict except EEPROM busy reads, %" PRIu64 " of which differ,"
                        " and the controls (MAME's), %" PRIu64 " reads\n", io.do_mismatch, io.input_reads);
        if (rt::kCrypt5881)
            std::printf("  315-5881 (native): %" PRIu64 " register reads identical to MAME\n", crypt.reads);
        if (tgplog)
            std::printf("  TGP's buffer RAM reads (MAME's): %" PRIu64 " of %" PRIu64 " differ%s%s\n", tgplog->mismatch, tgplog->reads,
                        tgplog->mismatch ? "; first " : "", tgplog->first.c_str());
        if (geo.buffer_mismatch) std::printf("  first buffer RAM difference: %s\n", geo.first_buffer_mismatch.c_str());
        if (geo.status_mismatch) std::printf("  first FIFO-status difference: %s\n", geo.first_status_mismatch.c_str());
        return 0;
    } catch (const rt::Divergence &d) {
        std::printf("m2native: DIVERGED after %" PRIu64 " instructions: %s\n", done, d.what());
        return 1;
    } catch (const rt::GeoFatal &f) {
        std::printf("m2native: GEOMETRIZER FATAL after %" PRIu64 " instructions: %s\n", done, f.what());
        return 1;
    } catch (const rt::TgpFatal &f) {
        std::printf("m2native: TGP FATAL after %" PRIu64 " instructions: %s\n", done, f.what());
        return 1;
    } catch (const rt::Fatal &f) {
        std::printf("m2native: FATAL after %" PRIu64 " instructions: %s\n", done, f.what());
        return 1;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "m2native: %s\n", e.what());
        return 2;
    }
}
