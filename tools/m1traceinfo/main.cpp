// m1traceinfo: what a Model 1 trace holds. Device accesses grouped by
// direction, space, address and size, with counts and the first few values.
//
//   m1traceinfo TRACE.m1tr [--from HEX --to HEX] [--dump N]
// --from/--to filter by address; --dump lists the first N matching events.
#include "model1/trace.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <tuple>
#include <vector>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: m1traceinfo TRACE.m1tr [--from HEX --to HEX]\n");
        return 2;
    }
    uint32_t from = 0, to = 0xffffffff;
    uint64_t dump = 0;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--from")) from = uint32_t(std::strtoul(argv[i + 1], nullptr, 16));
        else if (!std::strcmp(argv[i], "--to")) to = uint32_t(std::strtoul(argv[i + 1], nullptr, 16));
        else if (!std::strcmp(argv[i], "--dump")) dump = std::strtoull(argv[i + 1], nullptr, 0);
    }
    const auto ev = m1::read_trace(argv[1]);
    struct Stat {
        uint64_t n = 0, first = 0;
        std::vector<uint32_t> values;
    };
    std::map<std::tuple<uint8_t, uint8_t, uint32_t, uint8_t>, Stat> st;
    uint64_t samples = 0, irqs = 0, end = 0;
    for (const auto &e : ev) {
        if (e.type == m1::TraceEvent::Sample) ++samples;
        else if (e.type == m1::TraceEvent::Irq) ++irqs;
        else if (e.type == m1::TraceEvent::End) end = e.count;
        if (e.type != m1::TraceEvent::Read && e.type != m1::TraceEvent::Write) continue;
        if (e.addr < from || e.addr > to) continue;
        if (dump) {
            std::printf("%s\n", m1::describe(e).c_str());
            --dump;
        }
        Stat &s = st[{uint8_t(e.type), e.space, e.addr, e.size}];
        if (!s.n) s.first = e.count;
        ++s.n;
        if (s.values.size() < 6) s.values.push_back(e.data);
    }
    std::printf("%zu events, %" PRIu64 " samples, %" PRIu64 " interrupts, %" PRIu64 " instructions\n", ev.size(),
                samples, irqs, end);
    for (const auto &[k, s] : st) {
        const auto [type, space, addr, size] = k;
        std::printf("%s %s %06x/%u  %10" PRIu64 "  first at %" PRIu64 ":", type == m1::TraceEvent::Read ? "R" : "W",
                    space ? "io " : "mem", addr, size, s.n, s.first);
        for (uint32_t v : s.values) std::printf(" %x", v);
        std::printf("\n");
    }
    return 0;
}
