// license:BSD-3-Clause
// copyright-holders:R. Belmont, Olivier Galibert, ElSemi, Angelo Salese
//
// Model 2 geometry ports as the i960 sees them, transplanted from MAME's
// src/mame/sega/model2.cpp (copro_ctl1_w, copro_function_port_w,
// copro_fifo_r/w, geo_r/w, geo_prg_w, geo_ctl1_w, push_geo_data,
// copro_tgp_memory_r/w) at dddd73680656e355bb2b5beecab1167c9f07bf81
// (BSD-3-Clause; notice above kept as the licence requires). MAME's device
// FIFOs, halts and stalls are replaced by running the recompiled TGP on
// demand. Model 2B: model2b_state's copro_fifo_w, copro_boot,
// copro_status_r, copro_sharc_map and machine_start/machine_reset's FIFO
// flags, around the recompiled SHARC. Model 2C: model2c_state's
// copro_fifo_w, copro_boot, copro_tgpx4_data_map and machine_start's FIFOs,
// around the recompiled TGPx4. See THIRD_PARTY.md.

#include "runtime/m2_tgp_board.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rt {

namespace {
uint32_t crc32_words(const std::vector<uint32_t> &w) {
    uint32_t c = 0xffffffffu;
    for (uint32_t v : w)
        for (int b = 0; b < 4; b++) {
            c ^= (v >> (8 * b)) & 0xff;
            for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
        }
    return ~c;
}

#if M2_MODEL2B
uint32_t crc32_u16(const std::vector<uint16_t> &w) {
    uint32_t c = 0xffffffffu;
    for (uint16_t v : w)
        for (int b = 0; b < 2; b++) {
            c ^= (v >> (8 * b)) & 0xff;
            for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
        }
    return ~c;
}
#endif

std::vector<uint32_t> words(const std::vector<uint8_t> &b) {
    std::vector<uint32_t> w(b.size() / 4);
    std::memcpy(w.data(), b.data(), w.size() * 4);
    return w;
}
} // namespace

TgpBoard::TgpBoard(const std::vector<uint8_t> &tables, const std::vector<uint8_t> &copro_data)
    : tables_(words(tables)), copro_data_(words(copro_data)) {
#if M2_MODEL2B
    // No TGP tables. MAME machine_reset: the SHARC is reset and held until
    // the i960 boots it; FLAG0 (input FIFO empty) set, FLAG1 clear.
    if (copro_data_.size() != 0x200000) throw TgpFatal("bad copro data image");
    sharc_.bus = this;
    sharc_.reset();
    sharc_.set_flag_input(0, 1);
    sharc_.set_flag_input(1, 0);
#elif M2_MODEL2C
    // No TGP tables. MAME machine_reset: the TGPx4 is reset and held until
    // the i960 boots it.
    if (copro_data_.size() != 0x200000) throw TgpFatal("bad copro data image");
    tgpx4_.bus = this;
    tgpx4_.reset();
#else
    if (tables_.size() != 0x10000 || copro_data_.size() != 0x200000) throw TgpFatal("bad TGP table or copro data image");
#endif
    tgp_.bus = this;
#ifdef M2_FAST_TGP
    tgp_.in_fifo = &in_;
    tgp_.out_fifo = &out_;
#endif
    tgp_.tables = tables_.data();
    // MAME machine_reset: "initialize bufferram to a sane default".
    for (auto &w : buffer_) w = 0x07800f0f;
}

#ifdef M2_DC_MEMORY
TgpBoard::TgpBoard(const std::vector<uint8_t> &tables, RomSource &rom) : tables_(words(tables)), rom_(&rom) {
    if (tables_.size() != 0x10000 || rom.size(RomRegion::CoproData) != 0x800000)
        throw TgpFatal("bad TGP table or copro data image");
    tgp_.bus = this;
#ifdef M2_FAST_TGP
    tgp_.in_fifo = &in_;
    tgp_.out_fifo = &out_;
#endif
    tgp_.tables = tables_.data();
    for (auto &w : buffer_) w = 0x07800f0f;
}
#endif

void TgpBoard::coproctl_w(uint32_t data, uint32_t mask) {
#if M2_MODEL2B
    if ((data ^ coproctl_) == 0x80000000) {
        if (data & 0x80000000) { // start upload (model2b copro_halt does nothing)
            if (booted_) throw TgpFatal("the i960 uploads a second SHARC program while the first runs: not supported");
            coprocnt_ = 0;
            sharc_upload_.clear();
        } else { // copro_boot: the SHARC leaves halt at its reset vector
            if (sharc_upload_.size() != sharcgen::upload_words ||
                crc32_u16(sharc_upload_) != sharcgen::upload_crc32)
                throw TgpFatal("the i960 uploaded a SHARC program other than the one recompiled");
            booted_ = true;
            sharc_.held = false;
            run_tgp();
        }
    }
    coproctl_ = (coproctl_ & ~mask) | (data & mask);
    return;
#elif M2_MODEL2C
    if ((data ^ coproctl_) == 0x80000000) {
        if (data & 0x80000000) { // start upload (model2c copro_halt does nothing)
            if (booted_) throw TgpFatal("the i960 uploads a second TGPx4 program while the first runs: not supported");
            coprocnt_ = 0;
            tgpx4_upload_.clear();
        } else { // copro_boot: the TGPx4 leaves halt
            if (tgpx4_upload_.size() != tgpx4gen::upload_words || crc32_words(tgpx4_upload_) != tgpx4gen::upload_crc32)
                throw TgpFatal("the i960 uploaded a TGPx4 program other than the one recompiled");
            booted_ = true;
            run_tgp();
        }
    }
    coproctl_ = (coproctl_ & ~mask) | (data & mask);
    return;
#endif
    if ((data ^ coproctl_) == 0x80000000) {
        if (data & 0x80000000) { // start upload: the TGP is held
            upload_.clear();
            booted_ = false;
        } else { // boot: the TGP starts from reset on the uploaded program
            if (upload_.size() != tgpgen::program_words || crc32_words(upload_) != tgpgen::program_crc32)
                throw TgpFatal("the i960 uploaded a TGP program other than the one recompiled");
            for (size_t i = 0; i < upload_.size(); i++) tgp_.prog[i] = upload_[i];
            tgp_.reset();
            booted_ = true;
        }
    }
    coproctl_ = (coproctl_ & ~mask) | (data & mask);
}

void TgpBoard::function_port_w(uint32_t offset, uint32_t data) {
    uint32_t d = data & 0x800fffff;
    const uint32_t a = (offset >> 2) & 0xff;
    d |= a << 23;
    in_.push_back(d);
#if M2_MODEL2B || M2_MODEL2C
    run_tgp();
#endif
}

void TgpBoard::fifo_w(uint32_t data) {
#if M2_MODEL2B
    if (coproctl_ & 0x80000000) { // the host boot, 16 bits a word
        sharc_upload_.push_back(uint16_t(data));
        sharc_.external_dma_write(coprocnt_, data & 0xffff);
        coprocnt_++;
    } else {
        in_.push_back(data);
        run_tgp();
    }
    return;
#elif M2_MODEL2C
    if (coproctl_ & 0x80000000) { // the program, 32-bit halves of each 64-bit instruction (the RAM holds 4K)
        if (coprocnt_ < 0x2000) tgpx4_upload_.push_back(data);
        coprocnt_++;
    } else {
        in_.push_back(data);
        run_tgp();
    }
    return;
#endif
    if (coproctl_ & 0x80000000) {
        if (upload_.size() >= 0x1000) throw TgpFatal("TGP program upload past 4K words");
        upload_.push_back(data);
    } else {
        in_.push_back(data);
    }
}

void TgpBoard::run_tgp() {
#if M2_MODEL2B
    if (!booted_) return;
    sharc_.m_core->flag[0] = in_.empty();
    sharcgen::run(sharc_, UINT64_MAX); // until it waits on a FIFO flag
#elif M2_MODEL2C
    if (booted_) tgpx4gen::run(tgpx4_, UINT64_MAX); // until it stalls on its empty input FIFO
#else
    if (booted_) tgpgen::run(tgp_, UINT64_MAX); // until it waits on an empty input FIFO
#endif
}

uint32_t TgpBoard::fifo_r() {
    if (out_.empty()) run_tgp();
    if (out_.empty()) throw TgpFatal("the i960 read the TGP's output FIFO, but the TGP has consumed all its input and produced nothing");
    const uint32_t v = out_.front();
    out_.pop_front();
#if M2_MODEL2B
    out_fifo_changed();
    run_tgp(); // a SHARC waiting on FLAG1 goes on
#endif
    return v;
}

bool TgpBoard::fifo_out_empty() {
    if (out_.empty()) run_tgp();
    return out_.empty();
}

bool TgpBoard::fifo_pop(uint32_t &v) {
    if (in_.empty()) return false;
    v = in_.front();
    in_.pop_front();
    return true;
}

uint32_t TgpBoard::mem_r(uint32_t adr) {
    if (oracle) {
        MemOracle *o = oracle;
        oracle = nullptr;
        const uint32_t v = o->mem_r(adr, mem_r(adr));
        oracle = o;
        return v;
    }
#ifdef M2_DC_MEMORY
    if (adr & 0x800000) return rom_->dword(RomRegion::CoproData, (adr & 0x1fffff) * 4);
#else
    if (adr & 0x800000) return copro_data_[adr & (copro_data_.size() - 1)];
#endif
    if (adr & 0x400000) return buffer_[adr & 0x7fff];
    return 0;
}

void TgpBoard::mem_w(uint32_t adr, uint32_t v) {
    if (adr & 0x400000) buffer_[adr & 0x7fff] = v;
}

#if M2_MODEL2B
void TgpBoard::iop_w(uint32_t offset, uint32_t data) {
    // Last Bronx uploads the SHARC's program through its EPB0 register
    // (IOP 4) instead of the boot FIFO: the same 16-bit words, DMA channel 6
    // packing them from 0x20000 as the host boot does.
    if ((coproctl_ & 0x80000000) && offset == 4) sharc_upload_.push_back(uint16_t(data));
    sharc_.external_iop_write(offset, data);
}

uint32_t TgpBoard::ext_r(uint32_t a) {
    if (a >= 0x0400000 && a <= 0x0bfffff) { // the input FIFO
        if (in_.empty()) throw TgpFatal("the SHARC read its empty input FIFO");
        const uint32_t v = in_.front();
        in_.pop_front();
        sharc_.m_core->flag[0] = in_.empty();
        return v;
    }
    if (a >= 0x1400000 && a <= 0x1bfffff) { // buffer RAM
        const uint32_t o = a & 0x7fff;
        return oracle ? oracle->mem_r(o, buffer_[o]) : buffer_[o];
    }
    if (a >= 0x1c00000 && a <= 0x1dfffff) return copro_data_[(a - 0x1c00000) & (copro_data_.size() - 1)];
    return 0; // unmapped
}

void TgpBoard::ext_w(uint32_t a, uint32_t v) {
    if (a >= 0x0c00000 && a <= 0x13fffff) { // the output FIFO
        out_.push_back(v);
        out_fifo_changed();
        return;
    }
    if (a >= 0x1400000 && a <= 0x1bfffff) buffer_[a & 0x7fff] = v;
}
#endif

#if M2_MODEL2C
uint32_t TgpBoard::in_pop() {
    const uint32_t v = in_.front();
    in_.pop_front();
    return v;
}

// copro_tgpx4_data_map: buffer RAM at 0x400000 (mirrored to 0x7fffff),
// copro_data at 0x800000-0x9fffff; the rest unmapped (reads 0).
uint32_t TgpBoard::ext_r(uint32_t a) {
    if (a >= 0x400000 && a < 0x800000) {
        const uint32_t o = a & 0x7fff;
        return oracle ? oracle->mem_r(o, buffer_[o]) : buffer_[o];
    }
    if (a >= 0x800000 && a < 0xa00000) return copro_data_[(a - 0x800000) & (copro_data_.size() - 1)];
    return 0;
}

void TgpBoard::ext_w(uint32_t a, uint32_t v) {
    if (a >= 0x400000 && a < 0x800000) buffer_[a & 0x7fff] = v;
}
#endif

void TgpBoard::buffer_w(uint32_t byte_offset, uint32_t data, uint32_t mask) {
    if (!in_.empty()) run_tgp(); // MAME's TGP is caught up: its writes come first
    uint32_t &w = buffer_[(byte_offset >> 2) & 0x7fff];
    w = (w & ~mask) | (data & mask);
}

void TgpBoard::push_geo(uint32_t data) {
    if (!in_.empty()) run_tgp(); // as buffer_w (Manx TT's TGP writes where the i960 pushes)
    const uint32_t i = geo_write_start_ / 4;
    if (i >= 0x8000) throw TgpFatal("geometrizer push past the end of buffer RAM");
    buffer_[i] = data;
    geo_write_start_ += 4;
}

void TgpBoard::geoctl_w(uint32_t data) { geoctl_ = data; }

void TgpBoard::geo_prg_w(uint32_t data) {
    if (geoctl_ & 0x80000000) return; // geometrizer program upload: its own microcode, not modelled
    push_geo(data);
}

uint32_t TgpBoard::geo_r(uint32_t offset) const {
    const uint32_t address = offset * 4;
    if (address == 0x2008) return geo_write_start_;
    if (address == 0x3008) return geo_read_start_;
    return 0;
}

void TgpBoard::geo_w(uint32_t offset, uint32_t data) {
    const uint32_t address = offset * 4;
    if (address < 0x1000) {
        if (data & 0x80000000) {
            push_geo((data & 0x800fffff) | (((address >> 4) & 0x3f) << 23));
        } else if ((address & 0xf) == 0) {
            uint32_t r = (data & 0x000fffff) | (((address >> 4) & 0x3f) << 23);
            if (((address >> 4) & 0xc0) && ((address >> 4) & 0x3f) == 1) r |= ((address >> 10) & 3) << 29;
            push_geo(r);
        }
    } else if (address == 0x1008) {
        geo_write_start_ = data & 0xfffff;
    } else if (address == 0x3008) {
        geo_read_start_ = data & 0xfffff;
    } else {
        throw TgpFatal("geometrizer write to an unknown register");
    }
}

} // namespace rt

namespace rt {

namespace {
bool buffer_ram(uint32_t a) { return a >= 0x00900000 && a <= 0x0097ffff; }
} // namespace

bool TgpBoardModel::claims(uint32_t a) const {
    return (a >= 0x00800000 && a <= 0x00807fff) || (a >= 0x00884000 && a <= 0x00887fff) ||
           (a >= 0x00880000 && a <= 0x00883fff) || buffer_ram(a) || (a >= 0x00980000 && a <= 0x0098000b)
#if M2_MODEL2B
           || (a >= 0x008c0000 && a <= 0x008c0fff) || a == 0x00980014
#elif M2_MODEL2C
           || a == 0x00980014
#endif
        ;
}

DeviceModel::Check TgpBoardModel::read(uint32_t addr, uint32_t mask, uint32_t &value) {
    if (buffer_ram(addr)) {
        ++buffer_reads;
        b_.sync(); // as M2Board::dev_read
        value = b_.buffer_r(addr & 0x1ffff);
        return Measured;
    }
    if (addr >= 0x00884000 && addr <= 0x00887fff) {
        // MAME's copro_fifo_r is a 32-bit handler: a 16-bit read still pops a
        // whole word and the bus keeps the lane asked for (races do this).
        (void)mask;
        ++fifo_words;
        value = b_.fifo_r();
        return Strict;
    }
    if (addr >= 0x00800000 && addr <= 0x00803fff) { value = b_.geo_r((addr - 0x00800000) >> 2); return Strict; }
    if (addr >= 0x00804000 && addr <= 0x00807fff) { value = 0xffffffffu; return Strict; } // geo_prg_r
    if (addr == 0x00980000) { value = b_.coproctl_r(); return Strict; }
#if M2_MODEL2B || M2_MODEL2C
    if (addr == 0x00980014) { value = b_.status_r(); return Strict; }
#endif
    if (addr == 0x00980004) {
        ++status_reads;
        value = b_.fifo_out_empty() ? 1 : 0;
        return Measured;
    }
    value = 0; // write-only ports (function port, geoctl) read as unmapped
    return Strict;
}

void TgpBoardModel::write(uint32_t addr, uint32_t data, uint32_t mask) {
    if (buffer_ram(addr)) { b_.buffer_w(addr & 0x1ffff, data, mask); return; }
    if (addr == 0x00980000) { b_.coproctl_w(data, mask); return; }
    // The other ports are MAME 32-bit handlers without a mask: a narrower
    // write reaches them as the whole dword, lanes not written being zero.
    data &= mask;
    if (addr >= 0x00884000 && addr <= 0x00887fff) b_.fifo_w(data);
    else if (addr >= 0x00880000 && addr <= 0x00883fff) b_.function_port_w((addr - 0x00880000) >> 2, data);
    else if (addr >= 0x00804000 && addr <= 0x00807fff) b_.geo_prg_w(data);
    else if (addr >= 0x00800000 && addr <= 0x00803fff) b_.geo_w((addr - 0x00800000) >> 2, data);
    else if (addr == 0x00980008) b_.geoctl_w(data);
#if M2_MODEL2B
    else if (addr >= 0x008c0000 && addr <= 0x008c0fff) b_.iop_w((addr - 0x008c0000) >> 2, data);
#endif
    // 0x00980004 (read-only status) ignores writes
}

void TgpBoardModel::measured_mismatch(uint32_t addr, uint32_t ours, uint32_t mame) {
    char b[96];
    std::snprintf(b, sizeof b, "%08x: ours %08x, MAME %08x (TGP at %llu instructions)", addr, ours, mame,
                  (unsigned long long)b_.tgp_instructions());
    static FILE *log = std::getenv("M2NATIVE_MISMATCH_LOG") ? std::fopen(std::getenv("M2NATIVE_MISMATCH_LOG"), "w") : nullptr;
    if (log) std::fprintf(log, "%s\n", b);
    if (buffer_ram(addr)) {
        if (!buffer_mismatch++) first_buffer_mismatch = b;
    } else {
        if (!status_mismatch++) first_status_mismatch = b;
    }
}

} // namespace rt
