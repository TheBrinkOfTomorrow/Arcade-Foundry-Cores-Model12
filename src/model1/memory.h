// Model 1 main board memory as the V60 sees it (MAME sega/model1.cpp,
// model1_mem / model1_comm_mem): ROM, the battery-backed RAM, work RAM, the
// TGP display lists, the System 24 tile and character RAM, the palette and
// colour translation RAM. These are host arrays the V60 reads inline;
// everything else on the bus is a device.
#pragma once

#include "v60/core.h"

#include <cstdint>
#include <iterator>
#include <vector>

namespace m1 {

struct RamRegion {
    uint32_t base, bytes;
    uint8_t fill; // power-on contents (the NVRAM is MAME's DEFAULT_ALL_1)
};

// Order is the order of a trace sample's hashes.
inline constexpr RamRegion kRamRegions[] = {
    {0x400000, 0x10000, 0xff}, // RAMA, battery-backed
    {0x500000, 0x40000, 0},    // RAMB
    {0x600000, 0x20000, 0},    // display lists 0 and 1
    {0x700000, 0x10000, 0},    // tile RAM
    {0x780000, 0x80000, 0},    // character RAM
    {0x900000, 0x4000, 0},     // palette
    {0x910000, 0xc000, 0},     // colour translation
};

class Memory {
public:
    // maincpu: the 32 MiB region from rom_set (program ROMs and data ROM banks)
    explicit Memory(std::vector<uint8_t> maincpu);
    void map(v60::Bus &bus);
    // Save states: the RAM regions and the data ROM bank (mapped again on load).
    template <typename S> void state(S &s, v60::Bus &bus) {
        for (auto &r : regions_) s.fixed(r);
        s(bank_);
        if (s.loading() && s.ok())
            for (int p = 0x10; p <= 0x1f; p++) bus.rd[p] = rom_.data() + bank_ + ((p - 0x10) << 16);
    }
    // E00004 (bank_w): data ROM bank at 100000-1fffff
    void bank_write(v60::Bus &bus, uint32_t data);
    // RAM at a V60 address; each region is contiguous host memory
    uint8_t *ram(uint32_t addr) { return pages_[addr >> 16] + (addr & 0xffff); }
    const uint8_t *ram(uint32_t addr) const { return pages_[addr >> 16] + (addr & 0xffff); }
    // FNV-1a-64 over a RAM region's little-endian 32-bit words
    uint64_t hash(const RamRegion &r) const;
    const std::vector<uint8_t> &rom() const { return rom_; }

private:
    std::vector<uint8_t> rom_;
    std::vector<uint8_t> regions_[std::size(kRamRegions)];
    uint8_t *pages_[256] = {}; // RAM pages (64 KiB each, some partly used), into regions_
    uint32_t bank_ = 0x1000000;
};

} // namespace m1
