#include "model1/memory.h"

#include <cstring>

namespace m1 {

Memory::Memory(std::vector<uint8_t> maincpu) : rom_(std::move(maincpu)) {
    for (size_t i = 0; i < std::size(kRamRegions); i++) {
        const RamRegion &r = kRamRegions[i];
        const uint32_t first = r.base >> 16, last = (r.base + r.bytes - 1) >> 16;
        regions_[i].assign(size_t(last - first + 1) << 16, r.fill);
        for (uint32_t p = first; p <= last; p++) pages_[p] = regions_[i].data() + (size_t(p - first) << 16);
    }
}

void Memory::map(v60::Bus &bus) {
    for (int p = 0; p < 256; p++) {
        bus.rd[p] = bus.wr[p] = nullptr;
        if (pages_[p]) bus.rd[p] = bus.wr[p] = pages_[p];
    }
    for (int p = 0x00; p <= 0x0f; p++) bus.rd[p] = rom_.data() + (p << 16);
    for (int p = 0x20; p <= 0x2f; p++) bus.rd[p] = rom_.data() + (p << 16);
    for (int p = 0xf8; p <= 0xff; p++) bus.rd[p] = rom_.data() + (p << 16);
    for (int p = 0x10; p <= 0x1f; p++) bus.rd[p] = rom_.data() + bank_ + ((p - 0x10) << 16);
}

void Memory::bank_write(v60::Bus &bus, uint32_t data) {
    // MAME's bank_w: only the low byte; 0x1 selects the 100000 window
    if ((data & 0xf) == 0x1) {
        bank_ = 0x1000000 + 0x100000 * ((data >> 4) & 0x7);
        for (int p = 0x10; p <= 0x1f; p++) bus.rd[p] = rom_.data() + bank_ + ((p - 0x10) << 16);
    }
}

uint64_t Memory::hash(const RamRegion &r) const {
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint32_t o = 0; o < r.bytes; o += 4) {
        const uint32_t a = r.base + o;
        uint32_t w;
        std::memcpy(&w, pages_[a >> 16] + (a & 0xffff), 4);
        h = (h ^ w) * 0x100000001b3ull;
    }
    return h;
}

} // namespace m1
