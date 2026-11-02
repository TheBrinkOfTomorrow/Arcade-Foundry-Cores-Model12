// The i960's side of the Model 2 geometry hardware: the TGP's FIFOs and
// control register, the geometrizer's command port, and buffer RAM, which
// all three write (MAME model2.cpp: copro_*, geo_*, push_geo_data). The TGP
// itself is the recompiled program (rt::tgpgen), run only when the i960
// needs its output: clockless, it returns when its input FIFO is empty.
//
// Model 2B (M2_MODEL2B): the same ports in front of a SHARC (rt::sharc, its
// program recompiled as rt::sharcgen), which the i960 boots through its
// host port (MAME model2b copro_*, copro_sharc_map). The SHARC polls its
// FIFO flags (FLAG0: input empty; FLAG1, as MAME wires it: the input FIFO
// full when the output FIFO was last pushed or popped); it runs whenever
// the i960 gives it a word, until it waits on a flag again.
//
// Model 2C (M2_MODEL2C): the same ports in front of a TGPx4 (rt::tgpx4, its
// program recompiled as rt::tgpx4gen), whose program the i960 writes through
// the FIFO port (MAME model2c copro_*, copro_tgpx4_data_map); it runs
// whenever the i960 gives it a word, until it stalls on its empty input FIFO.
#pragma once

#include "runtime/rom_source.h"
#include "runtime/tgp.h"
#if M2_MODEL2B
#include "runtime/sharc/sharc_gen.h"
#endif
#if M2_MODEL2C
#include "runtime/tgpx4/tgpx4_gen.h"
#endif

#include <cstdint>
#include <deque>
#include <vector>

namespace rt {


class TgpBoard : public TgpBus
#if M2_MODEL2B
    , public sharc::SharcBus
#endif
#if M2_MODEL2C
    , public tgpx4::Tgpx4Bus
#endif
{
public:
    // tables: copro_tgp_tables (0x40000 bytes); copro_data: 0x800000 bytes.
    TgpBoard(const std::vector<uint8_t> &tables, const std::vector<uint8_t> &copro_data);
#ifdef M2_DC_MEMORY
    // copro_data read through pages (RomRegion::CoproData), not copied.
    TgpBoard(const std::vector<uint8_t> &tables, RomSource &rom);
#endif
#ifdef M2_FAST_TGP
    // The TGP holds pointers to in_ and out_: a copy would read the original's.
    TgpBoard(const TgpBoard &) = delete;
    TgpBoard &operator=(const TgpBoard &) = delete;
#endif

    // i960 accesses (dword offsets within each range, MAME's handlers).
    void function_port_w(uint32_t offset, uint32_t data);  // 0x00880000
    void fifo_w(uint32_t data);                           // 0x00884000
    uint32_t fifo_r();                                    // 0x00884000
    bool fifo_out_empty();                                // 0x00980004 (after running the TGP)
    void coproctl_w(uint32_t data, uint32_t mask);        // 0x00980000
    uint32_t coproctl_r() const { return coproctl_; }
    void geo_w(uint32_t offset, uint32_t data);           // 0x00800000
    uint32_t geo_r(uint32_t offset) const;
    void geo_prg_w(uint32_t data);                        // 0x00804000
    void geoctl_w(uint32_t data);                         // 0x00980008
#if M2_MODEL2B || M2_MODEL2C
    uint32_t status_r() const { return coprocnt_ == 0 ? 0xffffffffu : 0; } // 0x00980014 (copro_status_r)
#endif
#if M2_MODEL2B
    void iop_w(uint32_t offset, uint32_t data);                            // 0x008c0000 (the SHARC's IOP registers)
#endif
    uint32_t buffer_r(uint32_t byte_offset) const { return buffer_[(byte_offset >> 2) & 0x7fff]; }
    void buffer_w(uint32_t byte_offset, uint32_t data, uint32_t mask);

    const uint32_t *buffer() const { return buffer_; }
    uint32_t *buffer_data() { return buffer_; }             // the geometrizer reads its display list here
    uint32_t geo_read_start() const { return geo_read_start_; }
#if M2_MODEL2B
    uint64_t tgp_instructions() const { return sharc_.count; }
#elif M2_MODEL2C
    uint64_t tgp_instructions() const { return tgpx4_.count; }
#else
    uint64_t tgp_instructions() const { return tgp_.count; }
#endif
    bool booted() const { return booted_; }
    // Run the TGP until it waits on an empty input FIFO. The game polls the
    // TGP's mailbox in buffer RAM for results; the TGP must have caught up.
    void sync() { run_tgp(); }

    // The lockstep's oracle for the TGP's reads of external memory: when
    // the i960 writes buffer RAM where the TGP reads, the value read
    // depends on how far MAME's TGP had run (m2native, M2NATIVE_TGPLOG).
    struct MemOracle {
        virtual uint32_t mem_r(uint32_t adr, uint32_t ours) = 0;
    };
    MemOracle *oracle = nullptr;

    // TgpBus (the TGP's side)
    bool fifo_pop(uint32_t &v) override;
    void fifo_push(uint32_t v) override { out_.push_back(v); }
    uint32_t mem_r(uint32_t adr) override;
    void mem_w(uint32_t adr, uint32_t v) override;
#if M2_MODEL2B
    // SharcBus (the SHARC's external port, MAME copro_sharc_map)
    uint32_t ext_r(uint32_t address) override;
    void ext_w(uint32_t address, uint32_t data) override;
#endif
#if M2_MODEL2C
    // Tgpx4Bus (the TGPx4's FIFOs, MAME's 8 words deep, and its external
    // bus, MAME copro_tgpx4_data_map)
    bool in_empty() override { return in_.empty(); }
    bool in_full() override { return in_.size() >= 8; }
    uint32_t in_pop() override;
    void in_clear() override { in_.clear(); }
    bool out_empty() override { return out_.empty(); }
    bool out_full() override { return out_.size() >= 8; }
    void out_push(uint32_t v) override { out_.push_back(v); }
    void out_clear() override { out_.clear(); }
    uint32_t ext_r(uint32_t address) override;
    void ext_w(uint32_t address, uint32_t data) override;
#endif

private:
    void run_tgp();
    void push_geo(uint32_t data);

    Tgp tgp_;
    std::vector<uint32_t> tables_, copro_data_;
#ifdef M2_DC_MEMORY
    RomSource *rom_ = nullptr;
#endif
#ifdef M2_FAST_TGP
    WordFifo in_, out_;
#else
    std::deque<uint32_t> in_, out_;
#endif
    std::vector<uint32_t> upload_;
    uint32_t coproctl_ = 0, geoctl_ = 0, geo_write_start_ = 0, geo_read_start_ = 0;
    bool booted_ = false;
#if M2_MODEL2B
    sharc::Sharc sharc_;
    uint32_t coprocnt_ = 0; // MAME m_coprocnt: words of the host boot upload
    std::vector<uint16_t> sharc_upload_;
    void out_fifo_changed() { sharc_.m_core->flag[1] = in_.size() >= 16; } // MAME's FLAG1 (input FIFO full)
#endif
#if M2_MODEL2C
    tgpx4::Tgpx4 tgpx4_;
    uint32_t coprocnt_ = 0;               // MAME m_coprocnt: 32-bit halves of the upload
    std::vector<uint32_t> tgpx4_upload_;
#endif
    uint32_t buffer_[0x8000];
};

} // namespace rt

#include "runtime/m2_replay_bus.h"

#include <string>

namespace rt {

// TgpBoard behind the replay bus: the i960's accesses to the geometry ports,
// TGP FIFOs and buffer RAM go to the native model. TGP output words are
// strict (must equal MAME's). Buffer RAM reads and FIFO-status polls depend
// on how far the TGP has run relative to the i960, so they are measured.
class TgpBoardModel : public DeviceModel {
public:
    explicit TgpBoardModel(TgpBoard &b) : b_(b) {}
    bool claims(uint32_t a) const override;
    Check read(uint32_t addr, uint32_t mask, uint32_t &value) override;
    void write(uint32_t addr, uint32_t data, uint32_t mask) override;
    void measured_mismatch(uint32_t addr, uint32_t ours, uint32_t mame) override;
    const uint8_t *region(uint32_t base) const override {
        return base == 0x00900000 ? reinterpret_cast<const uint8_t *>(b_.buffer()) : nullptr;
    }

    uint64_t fifo_words = 0, buffer_reads = 0, status_reads = 0;
    uint64_t buffer_mismatch = 0, status_mismatch = 0;
    std::string first_buffer_mismatch, first_status_mismatch;

private:
    TgpBoard &b_;
};

} // namespace rt
