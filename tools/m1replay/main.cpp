// m1replay: the Model 1 trace harness with MAME's V60 executor (the
// reference interpreter, src/v60/interp). Every device access, interrupt and
// sample is checked against the MAME trace; RAM and ROM are native. It checks
// the trace and the board's memory map before the recompiled code runs
// against the same trace (m1native).
//
//   m1replay ROMSET.zip|7z|DIR TRACE.m1tr [--set vr]

#include "model1/replay_bus.h"
#include "model1/rom_set.h"
#include "v60/interp.h"

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: m1replay ROMSET TRACE.m1tr [--set NAME]\n");
        return 2;
    }
    std::string set_name = "vr";
    for (int i = 3; i + 1 < argc; i++)
        if (!std::strcmp(argv[i], "--set")) set_name = argv[i + 1];
    uint64_t done = 0;
    try {
        const m1::RomSet *set = m1::find_rom_set(set_name);
        if (!set) throw std::runtime_error("unknown set " + set_name);
        m1::Images img = m1::load_rom_set(*set, argv[1]);
        m1::Memory mem(std::move(img.at("maincpu")));
        m1::ReplayBus bus(mem, m1::read_trace(argv[2]));
        v60::Core core(&bus);
        bus.attach(&core);
        uint64_t count = 0;
        bus.set_counter(&count);
        core.reset();
        bus.take_init(core.m_reg);
        const auto t0 = std::chrono::steady_clock::now();
        while (!bus.finished()) {
            int vector;
            uint32_t pc;
            if (bus.take_irq(vector, pc)) {
                if (pc != core.m_reg[32] || !core.interrupts_enabled()) {
                    char b[128];
                    std::snprintf(b, sizeof b, "MAME took interrupt %d at PC %08x; ours is at %08x, PSW %08x", vector,
                                  pc, core.m_reg[32], core.psw());
                    throw m1::Divergence(b);
                }
                core.take_interrupt(vector);
            }
            v60::Interp::step(core);
            done = ++count;
        }
        const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("m1replay: MATCH. %" PRIu64 " instructions, %" PRIu64 " device events, %" PRIu64
                    " interrupts, %" PRIu64 " samples identical to MAME; %.2f s\n",
                    done, bus.events(), bus.irqs(), bus.samples(), s);
        return 0;
    } catch (const m1::Divergence &d) {
        std::printf("m1replay: DIVERGED after %" PRIu64 " instructions: %s\n", done, d.what());
        return 1;
    } catch (const std::exception &e) {
        std::printf("m1replay: FATAL after %" PRIu64 " instructions: %s\n", done, e.what());
        return 1;
    }
}
