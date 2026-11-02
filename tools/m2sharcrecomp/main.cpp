// m2sharcrecomp: static recompiler for the Model 2B SHARC (ADSP-21062) program.
//
//   m2sharcrecomp SHARC_UPLOAD.bin OUT.cpp
//
// The SHARC has no ROM of its own: at boot the i960 writes its program, 16
// bits at a time, through the SHARC's host boot DMA (12,288 words, 4,096
// 48-bit instructions at 0x20000, for Indy 500; scripts/m2import.py extracts
// them from the game's ROM as sharc_upload.bin, little-endian 16-bit words).
// This tool packs the words as the boot DMA does (16/48 packing,
// least-significant word first) and turns every instruction into native C++:
// one label per program address, each running one step of MAME's
// execute_run (the pc/daddr/faddr/nfaddr pipeline, the ASTAT history, the
// loop test, the system register latency) around MAME's handler for the
// instruction (src/runtime/sharc/mame/ops.inc), with the opcode a constant
// the compiler folds into the handler. The next instruction is the
// pipeline's daddr: a direct goto when it is the next address in the same
// chunk of the program (16 addresses, one function), the dispatch otherwise
// (jumps, calls, returns, loops, delayed branches).
//
// The loop test runs only at the addresses DO instructions name as loop
// ends (the only values laddr takes). Waits are found statically: a jump to
// itself on a FLAG input (the FIFO status lines), or a one-instruction DO
// loop on one, returns from run() when it repeats, to resume there once
// the board has changed the flag; repeating it changes no state, as MAME's
// SHARC spinning there does not. Nothing is interpreted at run time; an
// opcode MAME does not implement compiles to MAME's error.
//
// The output is derived from the game: write it under build/ (git-ignored),
// never commit it.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct OpRow {
    uint32_t mask, bits;
    const char *name;
};
const OpRow kOps[] = {
#include "runtime/sharc/mame/optable.inc"
};

const char *handler(uint64_t op) {
    const uint32_t top = uint32_t((op >> 39) & 0x1ff) << 7; // MAME build_opcode_table
    for (const OpRow &r : kOps)
        if ((r.mask & top) == r.bits) return r.name;
    return "sharcop_unimplemented";
}

int64_t sext24(uint64_t v) { return int64_t(int32_t(uint32_t(v & 0xffffff) << 8) >> 8); }
unsigned cond_of(uint64_t op) { return unsigned((op >> 33) & 0x1f); } // op_get_cond
bool flag_cond(unsigned c) { return (c >= 0x09 && c <= 0x0c) || (c >= 0x19 && c <= 0x1c); }

uint32_t crc32(const std::vector<uint8_t> &d) {
    uint32_t c = 0xffffffffu;
    for (uint8_t x : d) {
        c ^= x;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1)));
    }
    return ~c;
}

std::string hx(uint64_t v) {
    char b[24];
    std::snprintf(b, sizeof b, "0x%llx", (unsigned long long)v);
    return b;
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: m2sharcrecomp SHARC_UPLOAD.bin OUT.cpp\n");
        return 2;
    }
    try {
        std::ifstream f(argv[1], std::ios::binary);
        if (!f) throw std::runtime_error(std::string("cannot open ") + argv[1]);
        const std::vector<uint8_t> raw{std::istreambuf_iterator<char>(f), {}};
        if (raw.empty() || raw.size() % 2) throw std::runtime_error("the upload is not a whole number of 16-bit words");
        // Virtual-On's ends a word into an instruction (11,038 words): MAME's
        // DMA packs it into the low 16 bits over internal RAM's zeroes.
        const size_t n = (raw.size() + 5) / 6;
        if (n > 0x5000) throw std::runtime_error("the upload is larger than internal memory block 0");
        constexpr uint32_t kBase = 0x20000;
        std::vector<uint64_t> prog(n, 0);
        for (size_t w = 0; w < raw.size() / 2; w++) {
            const uint64_t v = raw[2 * w] | (uint64_t(raw[2 * w + 1]) << 8);
            prog[w / 3] |= v << (16 * (w % 3)); // external_dma_write, pmode 2, MSWF 0
        }

        // Loop ends: the address every DO instruction names.
        std::set<uint32_t> loop_ends, spins;
        for (size_t i = 0; i < n; i++) {
            const uint32_t pc = kBase + uint32_t(i);
            const std::string h = handler(prog[i]);
            if (h == "sharcop_do_until" || h == "sharcop_do_until_counter_imm" || h == "sharcop_do_until_counter_ureg") {
                const uint32_t end = uint32_t(pc + sext24(prog[i]));
                loop_ends.insert(end);
                // DO end UNTIL <flag> with end = pc + 1 and a NOP body: a wait.
                if (h == "sharcop_do_until" && end == pc + 1 && flag_cond(cond_of(prog[i])) && end < kBase + n &&
                    prog[end - kBase] == 0)
                    spins.insert(end);
            }
            // IF <flag> JUMP (here), not delayed: a wait.
            const bool delayed = (prog[i] >> 26) & 1; // op_get_jump_j
            if (!delayed && flag_cond(cond_of(prog[i])) &&
                ((h == "sharcop_direct_jump" && (prog[i] & 0xffffff) == pc) ||
                 (h == "sharcop_relative_jump" && (prog[i] & 0xffffff) == 0)))
                spins.insert(pc);
        }

        // The program in chunks of kChunk addresses, one function each (a
        // compiler's optimiser is superlinear in a function's size): labels
        // for every address, entered at any of them, left when the pipeline's
        // next address is outside the chunk.
        constexpr size_t kChunk = 16;
        const size_t chunks = (n + kChunk - 1) / kChunk;
        std::ostringstream o;
        o << "// Generated by tools/m2sharcrecomp from the SHARC program the i960 uploads.\n"
             "// Derived from the game's ROM: never commit it.\n"
             "#include \"runtime/sharc/sharc_gen.h\"\n"
             "#include \"runtime/sharc/sharc_impl.h\"\n\n"
             "namespace rt::sharcgen {\n\n"
             "const uint32_t upload_words = "
          << raw.size() / 2 << ";\nconst uint32_t upload_crc32 = " << hx(crc32(raw)) << "u;\nconst uint32_t program_base = " << hx(kBase)
          << "u, program_size = " << n << ";\n\n"
             "namespace {\n\n"
             "enum class Next { Dispatch, Wait, Idle, Budget };\n\n";
        for (size_t k = 0; k < chunks; k++) {
            const size_t lo = k * kChunk, hi = std::min(n, lo + kChunk);
            o << "Next chunk" << k << "(rt::sharc::Sharc &s, uint64_t &budget) {\n"
                 "    auto *const c = s.m_core;\n"
                 "    switch (c->daddr) {\n";
            for (size_t i = lo; i < hi; i++) o << "    case " << hx(kBase + i) << ": goto L" << std::hex << kBase + i << std::dec << ";\n";
            o << "    default: return Next::Dispatch;\n    }\n";
            for (size_t i = lo; i < hi; i++) {
                const uint32_t pc = kBase + uint32_t(i);
                const std::string h = handler(prog[i]);
                o << "L" << std::hex << pc << std::dec << ": {\n"
                  << "    c->pc = " << hx(pc) << "u;\n"
                     "    c->daddr = c->faddr;\n"
                     "    c->faddr = c->nfaddr;\n"
                     "    c->nfaddr++;\n"
                     "    c->astat_old_old_old = c->astat_old_old;\n"
                     "    c->astat_old_old = c->astat_old;\n"
                     "    c->astat_old = c->astat;\n"
                  << "    c->opcode = " << hx(prog[i]) << "ull;\n";
                if (loop_ends.count(pc)) o << "    s.loop_check();\n";
                o << "    s." << h << "();\n"
                  << "    if (c->systemreg_latency_cycles > 0 && --c->systemreg_latency_cycles <= 0) s.systemreg_write_latency_effect();\n"
                     "    s.dma_run_cycle(6);\n"
                     "    s.dma_run_cycle(7);\n"
                     "    s.count++;\n";
                if (spins.count(pc)) o << "    if (c->daddr == " << hx(pc) << "u) return Next::Wait;\n";
                if (h == "sharcop_idle") o << "    if (c->idle) return Next::Idle;\n";
                o << "    if (--budget == 0) return Next::Budget;\n";
                if (i + 1 < hi) o << "    if (c->daddr == " << hx(pc + 1) << "u) goto L" << std::hex << pc + 1 << std::dec << ";\n";
                o << "    return Next::Dispatch;\n}\n";
            }
            o << "}\n\n";
        }
        o << "using Chunk = Next (*)(rt::sharc::Sharc &, uint64_t &);\n"
             "const Chunk kChunks[] = {\n";
        for (size_t k = 0; k < chunks; k++) o << "    chunk" << k << ",\n";
        o << "};\n\n} // namespace\n\n"
             "Stop run(rt::sharc::Sharc &s, uint64_t budget) {\n"
             "    auto *const c = s.m_core;\n"
             "    if (c->idle) return Stop::Idle;\n"
             "    for (;;) {\n"
             "        const uint32_t i = c->daddr - "
          << hx(kBase) << "u;\n"
             "        if (i >= "
          << n << "u) throw rt::sharc::SharcFatal(\"SHARC: execution left the recompiled program\", c->daddr);\n"
             "        switch (kChunks[i / "
          << kChunk << "](s, budget)) {\n"
             "        case Next::Dispatch: break;\n"
             "        case Next::Wait: return Stop::Wait;\n"
             "        case Next::Idle: return Stop::Idle;\n"
             "        case Next::Budget: return Stop::Budget;\n"
             "        }\n"
             "    }\n"
             "}\n\n} // namespace rt::sharcgen\n";

        std::ofstream out(argv[2]);
        out << o.str();
        if (!out) throw std::runtime_error(std::string("cannot write ") + argv[2]);
        std::printf("m2sharcrecomp: %zu instructions, %zu loop ends, %zu waits\n", n, loop_ends.size(), spins.size());
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "m2sharcrecomp: %s\n", e.what());
        return 1;
    }
}
