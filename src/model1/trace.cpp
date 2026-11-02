#include "model1/trace.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <utility>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace m1 {

std::vector<TraceEvent> read_trace(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    const std::vector<uint8_t> d{std::istreambuf_iterator<char>(f), {}};
    if (d.size() < 8 || std::memcmp(d.data(), "M1TR", 4) != 0) throw std::runtime_error(path + ": not an M1TR trace");
    std::vector<TraceEvent> out;
    size_t p = 8;
    auto u32 = [&](size_t o) { uint32_t v; std::memcpy(&v, d.data() + o, 4); return v; };
    auto u64 = [&](size_t o) { uint64_t v; std::memcpy(&v, d.data() + o, 8); return v; };
    uint32_t stalls = 0; // retries of the read being stalled
    while (p + 5 <= d.size()) {
        const uint8_t type = d[p];
        const uint32_t n = u32(p + 1);
        const size_t q = p + 5;
        if (q + n > d.size()) break; // cut short (MAME killed): the rest is lost
        TraceEvent e{};
        e.type = TraceEvent::Type(type);
        switch (type) {
        case TraceEvent::Write:
        case TraceEvent::Read:
            e.count = u64(q);
            e.addr = u32(q + 8);
            e.data = u32(q + 12);
            e.size = d[q + 16];
            e.space = d[q + 17];
            if (type == TraceEvent::Read) e.stalls = std::exchange(stalls, 0);
            out.push_back(e);
            break;
        case TraceEvent::Stalled:
            // drop the stalled read (the last read at this count)
            for (size_t i = out.size(); i-- > 0;)
                if (out[i].type == TraceEvent::Read) {
                    stalls = out[i].stalls + 1;
                    out.erase(out.begin() + long(i));
                    break;
                }
            break;
        case TraceEvent::Irq:
            e.count = u64(q);
            e.addr = u32(q + 8);
            e.data = u32(q + 12);
            out.push_back(e);
            break;
        case TraceEvent::Sample: {
            e.count = u64(q);
            for (int i = 0; i < 34; i++) e.regs[i] = u32(q + 8 + 4 * i);
            const uint8_t nr = d[q + 8 + 34 * 4];
            for (int i = 0; i < nr; i++) e.hashes.push_back(u64(q + 8 + 34 * 4 + 1 + 16 * i + 8));
            out.push_back(e);
            break;
        }
        case TraceEvent::Init:
            for (int i = 0; i < 68; i++) e.regs[i] = u32(q + 4 * i);
            out.push_back(e);
            break;
        case TraceEvent::End:
            e.count = u64(q);
            out.push_back(e);
            break;
        default: break;
        }
        p = q + n;
    }
    return out;
}

std::string describe(const TraceEvent &e) {
    char b[160];
    switch (e.type) {
    case TraceEvent::Read:
    case TraceEvent::Write:
        std::snprintf(b, sizeof b, "%s%s %06x size %u data %08x at instruction %" PRIu64, e.space ? "I/O " : "",
                      e.type == TraceEvent::Read ? "read" : "write", e.addr, e.size, e.data, e.count);
        break;
    case TraceEvent::Irq:
        std::snprintf(b, sizeof b, "interrupt vector %u (PC %08x) at instruction %" PRIu64, e.addr, e.data, e.count);
        break;
    case TraceEvent::Sample: std::snprintf(b, sizeof b, "sample at instruction %" PRIu64, e.count); break;
    case TraceEvent::End: std::snprintf(b, sizeof b, "end at instruction %" PRIu64, e.count); break;
    default: std::snprintf(b, sizeof b, "event %u", e.type); break;
    }
    return b;
}

} // namespace m1
