// The Model 1 lockstep trace (M1TR), written by MAME with
// patches/mame/0006 (M1TRACE_LOG): every V60 access outside RAM and ROM,
// every interrupt taken, and a sample of the registers and RAM hashes at
// each write to the interrupt control register (E00000). Counts are V60
// instructions completed before the one that made the event (an IN that
// stalled on an empty FIFO and was repeated is counted once).
//
// Record: u8 type, u32 payload bytes, payload.
//   0x10 write / 0x11 read: u64 count, u32 addr, u32 data, u8 size, u8 space
//   0x12 stalled: u64 count (the read before it stalled; MAME repeats it)
//   0x30 interrupt: u64 count, u32 vector, u32 PC
//   0x02 init: u32 regs[68], the register file at the first instruction
//        (MAME leaves most of it unset at power-on; a replay starts from it)
//   0x01 sample: u64 count, u32 r0-r31, PC, PSW, u8 n, n x (u32 base, u32 bytes, u64 hash)
//   0x7f end: u64 count
// Traces are game-derived working files: never committed (rules.md 7).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace m1 {

struct TraceEvent {
    enum Type : uint8_t { Sample = 0x01, Init = 0x02, Write = 0x10, Read = 0x11, Stalled = 0x12, Irq = 0x30, End = 0x7f } type;
    uint64_t count = 0;
    uint32_t addr = 0, data = 0; // Irq: addr = vector, data = PC
    uint8_t size = 0, space = 0;
    uint32_t stalls = 0;         // Read: the times MAME's V60 retried it first (stalled I/O reads)
    uint32_t regs[68] = {};      // Sample: r0-r31, PC, PSW; Init: all 68
    std::vector<uint64_t> hashes;
};

// Reads a whole trace; stalled reads are dropped (the repeat that completed
// is the one a native build makes) and counted on it (stalls).
std::vector<TraceEvent> read_trace(const std::string &path);

std::string describe(const TraceEvent &e);

} // namespace m1
