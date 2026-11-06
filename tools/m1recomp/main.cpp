// m1recomp: the Model 1 V60 static recompiler (Model 1 plan, M1).
//
//   m1recomp ROMSET OUTDIR [--set vr] [--seeds FILE]... [--pcs FILE]... [--chunk N]
//            [--reduce FILE] [--speculate]
//
// --reduce writes the fewest of the seeds that reach the same code (what
// seeds/<set>.txt holds: MAME's harvest is every executed address).
// --speculate also takes in code no seed reaches but the program probably
// has (see speculate()): what follows a return or jump, and what words in
// the ROM point at.
//
// Every instruction reachable from the reset vector and the seeds (our
// seeds file; MAME's harvest of executed addresses, M1TRACE_PCS) becomes
// native C++. How an instruction is decoded and what it does are MAME's own
// code (src/v60/mame): the generator runs MAME's handler for it at recompile
// time, with hooks (Rec) that evaluate each operand decoder and addressing
// mode symbolically (sym.h). That yields the instruction's length, the call
// to make (the handler, past any opcode-table dispatch), the C++ for each
// operand (every mode, register and displacement resolved; only register
// contents and memory left to run time) and its successors (the handler run
// under 64 machine states: flags, registers and memory varied). The output
// calls the same handler, templated on a per-instruction hook class with
// that C++ in it, so the generated game runs MAME's semantics with no
// decoding left.
//
// No fallback: control reaching an address that was not recompiled is a
// hard error at run time naming it (add it to the seeds). Speculation makes
// that rarer: every address recompiled is the right code for its bytes, so
// a guess that is really data costs only size. The output is
// derived from the game: write it under build/ (git-ignored), never commit
// it (rules.md 6).

#include "model1/rom_set.h"
#include "sym.h"
#include "v60/core.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using v60::Am;
using v60::Core;

// ---------------------------------------------------------------------------
// Recorder: MAME's handler run at recompile time.

struct RecCtx {
    m1r::SymAm *sym = nullptr;
    uint32_t dummy = 0;
    std::vector<std::string> chain;            // handlers entered, in order
    std::vector<std::pair<std::string, std::string>> hooks; // (hook, emitted C++)
};
RecCtx *g = nullptr;

const char *am_name(Am k) {
    switch (k) {
    case Am::Read: return "Read";
    case Am::BitRead: return "BitRead";
    case Am::Addr: return "Addr";
    case Am::BitAddr: return "BitAddr";
    case Am::Write: return "Write";
    }
    return "?";
}

struct Rec {
    static void enter(Core &, const char *name) { g->chain.push_back(name); }
    static void add(const char *hook, std::string code) { g->hooks.emplace_back(hook, std::move(code)); }
    static uint32_t entry(Core &c, Am k, const char *hook) {
        uint32_t len = 0;
        std::string code = g->sym->Entry(c, k, g->dummy, &len);
        add(hook, code + "return " + std::to_string(len) + "u; ");
        return len;
    }
    static uint32_t ReadAM(Core &c) { return entry(c, Am::Read, "ReadAM"); }
    static uint32_t BitReadAM(Core &c) { return entry(c, Am::BitRead, "BitReadAM"); }
    static uint32_t ReadAMAddress(Core &c) { return entry(c, Am::Addr, "ReadAMAddress"); }
    static uint32_t BitReadAMAddress(Core &c) { return entry(c, Am::BitAddr, "BitReadAMAddress"); }
    static uint32_t WriteAM(Core &c) { return entry(c, Am::Write, "WriteAM"); }
#define R1(name)                                                                                                     \
    static void name(Core &c, Am k1, uint8_t d1) { add(#name, g->sym->name(c, k1, d1, g->dummy)); }
#define W1(name)                                                                                                     \
    static void name(Core &c, uint8_t d2) { add(#name, g->sym->name(c, d2, g->dummy)); }
#define R2(name)                                                                                                     \
    static void name(Core &c, Am k1, uint8_t d1, Am k2, uint8_t d2) {                                               \
        add(#name, g->sym->name(c, k1, d1, k2, d2, g->dummy));                                                      \
    }
    R1(F12DecodeFirstOperand)
    W1(F12WriteSecondOperand)
    R2(F12DecodeOperands)
    R1(F2DecodeFirstOperand)
    R1(F2DecodeSecondOperand)
    W1(F2WriteSecondOperand)
    R2(F7aDecodeOperands)
    R1(F7bDecodeFirstOperand)
    W1(F7bWriteSecondOperand)
    R2(F7bDecodeOperands)
    R2(F7cDecodeOperands)
#undef R1
#undef W1
#undef R2
};

// The signature of each hook in generated code (parameters unused: resolved)
const std::map<std::string, std::string> kHookSig = {
    {"ReadAM", "static uint32_t ReadAM(v60::Core &c)"},
    {"BitReadAM", "static uint32_t BitReadAM(v60::Core &c)"},
    {"ReadAMAddress", "static uint32_t ReadAMAddress(v60::Core &c)"},
    {"BitReadAMAddress", "static uint32_t BitReadAMAddress(v60::Core &c)"},
    {"WriteAM", "static uint32_t WriteAM(v60::Core &c)"},
    {"F12DecodeFirstOperand", "static void F12DecodeFirstOperand(v60::Core &c, v60::Am, uint8_t)"},
    {"F12WriteSecondOperand", "static void F12WriteSecondOperand(v60::Core &c, uint8_t)"},
    {"F12DecodeOperands", "static void F12DecodeOperands(v60::Core &c, v60::Am, uint8_t, v60::Am, uint8_t)"},
    {"F2DecodeFirstOperand", "static void F2DecodeFirstOperand(v60::Core &c, v60::Am, uint8_t)"},
    {"F2DecodeSecondOperand", "static void F2DecodeSecondOperand(v60::Core &c, v60::Am, uint8_t)"},
    {"F2WriteSecondOperand", "static void F2WriteSecondOperand(v60::Core &c, uint8_t)"},
    {"F7aDecodeOperands", "static void F7aDecodeOperands(v60::Core &c, v60::Am, uint8_t, v60::Am, uint8_t)"},
    {"F7bDecodeFirstOperand", "static void F7bDecodeFirstOperand(v60::Core &c, v60::Am, uint8_t)"},
    {"F7bWriteSecondOperand", "static void F7bWriteSecondOperand(v60::Core &c, uint8_t)"},
    {"F7bDecodeOperands", "static void F7bDecodeOperands(v60::Core &c, v60::Am, uint8_t, v60::Am, uint8_t)"},
    {"F7cDecodeOperands", "static void F7cDecodeOperands(v60::Core &c, v60::Am, uint8_t, v60::Am, uint8_t)"},
};

// Handlers that dispatch through a table on the instruction's second byte:
// the generated call goes past them, with the state they set.
bool is_dispatcher(const std::string &n) {
    return n == "op5F" || n == "op5C" || n == "op58" || n == "op59" || n == "op5A" || n == "op5B" || n == "op5D" ||
           n == "opC6" || n == "opC7";
}

// The recompile-time machine: ROM pages from the image, every other page a
// scratch buffer (no devices: their values are dummies like the registers).
class RecBus : public v60::Bus {
public:
    RecBus(const uint8_t *image) : image_(image) {
        scratch_.assign(0x10000, 0);
        for (int p = 0; p < 256; p++) {
            rd[p] = const_cast<uint8_t *>(image) + (p << 16);
            wr[p] = nullptr;
        }
    }
    void scratch(uint32_t seed) {
        for (int p = 0; p < 256; p++)
            if (!is_code_[p]) rd[p] = wr[p] = scratch_.data();
        // the per-seed contents are computed once; each run starts from a copy
        if (seed >= pristine_.size()) pristine_.resize(seed + 1);
        std::vector<uint8_t> &src = pristine_[seed];
        if (src.empty()) {
            src.resize(0x10000);
            for (size_t i = 0; i < src.size(); i += 4) {
                const uint32_t v = seed * 0x01000193u + uint32_t(i) * 0x9e3779b1u;
                std::memcpy(&src[i], &v, 4);
            }
        }
        std::memcpy(scratch_.data(), src.data(), src.size());
        // a RAM page holding code copied from ROM: the scratch with the copy
        // being analysed
        for (auto &[p, buf] : own_) {
            buf = scratch_;
            for (const auto &[to, c] : current_)
                if (int(to >> 16) == p) std::copy_n(image_ + to, c.bytes, buf.begin() + (to & 0xffff));
            rd[p] = wr[p] = buf.data();
        }
    }
    // Code the game copies into RAM and runs there (RomSet::code_copies):
    // the copy now in the image at its address
    void set_copy(const m1::CodeCopy &c) {
        current_[c.to] = c;
        own_[int(c.to >> 16)];
    }
    void set_code_pages(const std::set<int> &pages) {
        for (int p : pages) is_code_[p] = true;
    }
    uint32_t dev_read(uint32_t, unsigned, unsigned) override { return 0x5a5a5a5a; }
    void dev_write(uint32_t, uint32_t, unsigned, unsigned) override {}

private:
    std::vector<uint8_t> scratch_;
    std::vector<std::vector<uint8_t>> pristine_;
    bool is_code_[256] = {};
    const uint8_t *image_;
    std::map<uint32_t, m1::CodeCopy> current_; // by RAM address
    std::map<int, std::vector<uint8_t>> own_;
};

struct Insn {
    uint32_t pc = 0;
    uint32_t len = 0; // fall-through length (0 if it never falls through)
    std::string call; // the C++ call expression (with H as the hook class)
    std::string prelude;
    std::string name;
    std::vector<std::pair<std::string, std::string>> hooks;
    std::set<uint32_t> succ; // direct successors
    bool indirect = false;
    bool call_insn = false;  // pushes a return address (JSR, BSR, CALL)
    uint32_t ret = 0;        // its return address
    std::string error;
};

constexpr int kRuns = 64;

Insn analyse(const uint8_t *image, RecBus &bus, uint32_t pc) {
    Insn in;
    in.pc = pc;
    m1r::Code code{image, 0xffffff};
    std::set<uint32_t> seen;
    std::string hooks_text0;
    for (int run = 0; run < kRuns; run++) {
        m1r::SymAm sym(code);
        RecCtx ctx;
        ctx.sym = &sym;
        ctx.dummy = 0x00401000u + uint32_t(run) * 0x1234u; // inside RAM, distinct per run
        g = &ctx;
        bus.scratch(uint32_t(run));
        Core c(&bus);
        c.reset();
        const int regpat = run / 16;
        for (int i = 0; i < 31; i++)
            c.m_reg[i] = regpat == 0 ? 0u : regpat == 1 ? uint32_t(1 + i) : regpat == 3 ? 1u : 0x00500000u + uint32_t(i) * 0x40u + uint32_t(run);
        c.m_reg[31] = 0x00500800u + uint32_t(run) * 16u; // SP
        c.m_reg[33] = 0x10040000u;                       // PSW: interrupt stack, IE
        c.m_flags.Z = run & 1;
        c.m_flags.S = run >> 1 & 1;
        c.m_flags.OV = run >> 2 & 1;
        c.m_flags.CY = run >> 3 & 1;
        c.m_reg[32] = pc;
        uint32_t newpc;
        try {
            const uint8_t op = code.r8(pc);
            const uint32_t inc = (c.*v60::Tables<Rec>::s_OpCodeTable[op])();
            newpc = c.m_reg[32] + inc;
        } catch (const v60::Fatal &f) {
            in.error = f.what();
            g = nullptr;
            return in;
        }
        g = nullptr;
        std::string text;
        for (auto &h : ctx.hooks) text += h.first + ":" + h.second + "\n";
        if (run == 0) {
            hooks_text0 = text;
            in.hooks = ctx.hooks;
            // the call: past the last table dispatch
            size_t first = 0;
            for (size_t i = 0; i < ctx.chain.size(); i++)
                if (is_dispatcher(ctx.chain[i])) first = i + 1;
            if (first >= ctx.chain.size()) {
                in.error = "no handler after the opcode tables";
                return in;
            }
            const uint8_t b1 = code.r8(pc + 1);
            char pre[64] = "";
            std::string arg;
            for (size_t i = 0; i < first; i++) {
                const std::string &d = ctx.chain[i];
                if (d == "op5F" || d == "op5C") std::snprintf(pre, sizeof pre, "c.m_instflags = 0x%02x;", b1);
                else if (d == "opC6" || d == "opC7") arg = std::to_string(b1 & 0x1f);
                else std::snprintf(pre, sizeof pre, "c.m_subop = 0x%02x;", b1);
            }
            in.prelude = pre;
            in.name = ctx.chain[first];
            in.call = "c.template " + in.name + "<H>(" + arg + ")";
            for (const auto &n : ctx.chain)
                if (n == "opJSR" || n == "opBSR" || n == "opCALL") in.call_insn = true;
        } else if (text != hooks_text0) {
            in.error = "operand code depends on the machine state (handler " + in.name + ")";
            return in;
        }
        seen.insert(newpc);
        if (run == 0 && in.call_insn) in.ret = c.m_program->read_dword_unaligned(c.m_reg[31]) & 0xffffff;
    }
    // Successors: up to two fixed targets (fall-through, branch); more means
    // the target comes from registers or memory.
    if (seen.size() > 2) {
        in.indirect = true;
    } else {
        in.succ = seen;
    }
    return in;
}

std::string hex6(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "%06x", v);
    return b;
}

// Code the program probably has that no seed reaches: indirect targets
// MAME's runs never took. Candidates: the bytes just after an instruction
// that never falls through (a return, jump or branch: the next routine
// often starts there; its length is not known, so the next few addresses),
// and the aligned words in the program ROM that point into it (tables of
// routines). A candidate is kept when everything it reaches decodes, stays
// in the program and is not huge; anything else is dropped. What is kept
// is the right code for its bytes (each instruction is decoded at its own
// address), so a wrong guess is only dead code.
size_t speculate(const uint8_t *image, RecBus &bus, const std::set<int> &code_pages, std::map<uint32_t, Insn> &insns,
                 uint32_t prog_lo, uint32_t prog_hi) { // where the program runs from (RomSet::code_lo/hi)
    constexpr size_t kMaxReach = 3000;
    constexpr uint32_t kAfter = 8;
    auto in_prog = [&](uint32_t a) { return a >= prog_lo && a < prog_hi && code_pages.count(int(a >> 16)); };
    std::set<uint32_t> tried, cand;
    auto after_stops = [&](const std::map<uint32_t, Insn> &from) {
        for (const auto &[pc, in] : from) {
            if (!in.error.empty()) continue;
            // falls through: the next address is a successor, or the return address of a call
            bool falls = in.call_insn;
            for (uint32_t t : in.succ)
                if (t > pc && t <= pc + 16) falls = true;
            if (falls) continue;
            for (uint32_t d = 1; d <= kAfter; d++) cand.insert((pc + d) & 0xffffff);
        }
    };
    after_stops(insns);
    for (uint32_t a = prog_lo; a + 4 <= prog_hi; a += 4) {
        const uint32_t v = uint32_t(image[a]) | uint32_t(image[a + 1]) << 8 | uint32_t(image[a + 2]) << 16 | uint32_t(image[a + 3]) << 24;
        if ((v >> 24 == 0x00 || v >> 24 == 0xff) && in_prog(v & 0xffffff)) cand.insert(v & 0xffffff);
    }
    size_t kept = 0;
    while (!cand.empty()) {
        const uint32_t seed = *cand.begin();
        cand.erase(cand.begin());
        if (insns.count(seed) || !tried.insert(seed).second || !in_prog(seed)) continue;
        std::map<uint32_t, Insn> found;
        std::vector<uint32_t> w = {seed};
        bool ok = true;
        while (!w.empty() && ok) {
            const uint32_t pc = w.back();
            w.pop_back();
            if (insns.count(pc) || found.count(pc)) continue;
            if (!in_prog(pc) || found.size() >= kMaxReach) {
                ok = false;
                break;
            }
            Insn in = analyse(image, bus, pc);
            if (!in.error.empty()) {
                ok = false;
                break;
            }
            for (uint32_t s : in.succ) w.push_back(s & 0xffffff);
            if (in.call_insn) w.push_back(in.ret);
            found[pc] = in;
        }
        if (!ok) continue;
        ++kept;
        after_stops(found);
        insns.insert(found.begin(), found.end());
    }
    return kept;
}

std::vector<uint32_t> read_addresses(const std::string &path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::vector<uint32_t> out;
    std::string line;
    while (std::getline(f, line)) {
        const size_t h = line.find('#');
        if (h != std::string::npos) line.resize(h);
        std::istringstream s(line);
        std::string w;
        if (s >> w) out.push_back(uint32_t(std::stoul(w, nullptr, 16)));
    }
    return out;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: m1recomp ROMSET OUTDIR [--set vr] [--seeds FILE]... [--pcs FILE]... [--chunk N]\n");
        return 2;
    }
    std::string set_name = "vr";
    std::vector<std::string> seed_files;
    std::string reduce_path;
    size_t chunk = 1500;
    bool speculate_code = false;
    for (int i = 3; i < argc; i++)
        if (!std::strcmp(argv[i], "--speculate")) speculate_code = true;
    for (int i = 3; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--speculate")) {
            --i; // a flag
            continue;
        }
        if (!std::strcmp(argv[i], "--set")) set_name = argv[i + 1];
        else if (!std::strcmp(argv[i], "--chunk")) chunk = std::max<size_t>(1, std::strtoul(argv[i + 1], nullptr, 0));
        else if (!std::strcmp(argv[i], "--seeds") || !std::strcmp(argv[i], "--pcs")) seed_files.push_back(argv[i + 1]);
        else if (!std::strcmp(argv[i], "--reduce")) reduce_path = argv[i + 1];
        else {
            std::fprintf(stderr, "m1recomp: unknown option %s\n", argv[i]);
            return 2;
        }
    }
    try {
        const m1::RomSet *set = m1::find_rom_set(set_name);
        if (!set) throw std::runtime_error("unknown set " + set_name);
        m1::Images img = m1::load_rom_set(*set, argv[1]);
        const std::vector<uint8_t> &maincpu = img.at("maincpu");
        // The V60's 24-bit space; the data ROM window (100000) is not code.
        std::vector<uint8_t> image(maincpu.begin(), maincpu.begin() + 0x1000000);
        // 100000-1FFFFF: the data ROM bank MAME starts with (Wing War calls
        // a subroutine there)
        if (maincpu.size() >= 0x1100000) std::copy_n(maincpu.begin() + 0x1000000, 0x100000, image.begin() + 0x100000);
        RecBus bus(image.data());
        // ROM pages: everything else is scratch at recompile time
        std::set<int> code_pages;
        for (int p = 0x00; p <= 0x2f; p++) code_pages.insert(p);
        for (int p = 0xf8; p <= 0xff; p++) code_pages.insert(p);
        bus.set_code_pages(code_pages);
        // Code the game copies from ROM into RAM and runs there (Wing War's
        // backup RAM), each copy a variant of the code at its RAM address
        // (the game changes them with its mode). An instruction is known by
        // its key: its address, and for a copy's code the copy's index + 1
        // in the top byte (run_native picks the copy RAM holds).
        const std::vector<m1::CodeCopy> &copies = set->code_copies;
        auto key_of = [](uint32_t pc, int v) { return pc | uint32_t(v + 1) << 24; };
        auto in_copy = [&](uint32_t pc, int v) { return pc >= copies[v].to && pc < copies[v].to + copies[v].bytes; };
        // the keys an address can be: itself in ROM, every copy holding it
        // in RAM; from a copy's own code, that copy only
        auto keys_for = [&](uint32_t pc, int from_v) {
            std::vector<uint32_t> k;
            if (code_pages.count(int(pc >> 16))) k.push_back(pc);
            else if (from_v >= 0 && in_copy(pc, from_v)) k.push_back(key_of(pc, from_v));
            else
                for (int v = 0; v < int(copies.size()); v++)
                    if (in_copy(pc, v)) k.push_back(key_of(pc, v));
            return k;
        };
        int loaded = -1;
        auto load = [&](int v) { // the copy's bytes where it runs, for the analysis
            if (v < 0 || v == loaded) return;
            std::copy_n(maincpu.begin() + copies[v].from, copies[v].bytes, image.begin() + copies[v].to);
            bus.set_copy(copies[v]);
            loaded = v;
        };

        std::vector<uint32_t> work = {0xfffff0}; // reset: PC = FFFFFFF0 on the 24-bit bus
        std::set<uint32_t> seeds;
        for (const auto &f : seed_files)
            for (uint32_t a : read_addresses(f)) seeds.insert(a & 0xffffff);
        for (uint32_t a : seeds) {
            const std::vector<uint32_t> k = keys_for(a, -1);
            if (k.empty()) std::fprintf(stderr, "m1recomp: warning: seed %s: not in ROM or a copy\n", hex6(a).c_str());
            work.insert(work.end(), k.begin(), k.end());
        }
        std::map<uint32_t, Insn> insns; // by key
        std::vector<std::string> errors;
        std::map<uint32_t, uint32_t> from; // the first instruction that led to each
        std::set<uint32_t> after_call;     // reached only past a call's return (seeds aside)
        auto push = [&](uint32_t pc, uint32_t by, int v, bool ret) {
            const std::vector<uint32_t> k = keys_for(pc & 0xffffff, v);
            if (k.empty()) {
                const std::string e = hex6(pc & 0xffffff) + ": not in ROM (reached from " + hex6(by & 0xffffff) + ")";
                // from a copy's code: a seed's other copy (decoded where it never ran)
                if (v >= 0) std::fprintf(stderr, "m1recomp: warning: %s\n", e.c_str());
                else errors.push_back(e);
            }
            for (uint32_t t : k) {
                work.push_back(t);
                if (from.emplace(t, by).second && (ret || after_call.count(by))) after_call.insert(t);
            }
        };
        while (!work.empty()) {
            const uint32_t key = work.back();
            work.pop_back();
            if (insns.count(key)) continue;
            const uint32_t pc = key & 0xffffff;
            const int v = int(key >> 24) - 1;
            load(v);
            Insn in = analyse(image.data(), bus, pc);
            if (!in.error.empty()) {
                std::string path; // how the analysis got there
                for (uint32_t p = key, n = 0; from.count(p) && n < 24; p = from[p], n++) path += " < " + hex6(from[p] & 0xffffff);
                const std::string e = hex6(pc) + ": " + in.error + (path.empty() ? "" : "  (reached" + path + ")");
                // past a call that never returns (an error handler's data),
                // or a seed in a copy that is not the one it ran in: no code
                // there, as under MAME, which never runs it
                if (after_call.count(key) || v >= 0) std::fprintf(stderr, "m1recomp: warning: %s\n", e.c_str());
                else errors.push_back(e);
                insns[key] = in;
                continue;
            }
            for (uint32_t t : in.succ) push(t, key, v, false);
            if (in.call_insn) push(in.ret, key, v, true); // where the callee returns to
            insns[key] = in;
        }

        if (speculate_code) {
            const size_t before = insns.size();
            const size_t guesses = speculate(image.data(), bus, code_pages, insns, set->code_lo, set->code_hi);
            std::printf("m1recomp: speculation: %zu entry points, %zu instructions\n", guesses, insns.size() - before);
        }

        if (!reduce_path.empty()) {
            // Grow the code reached from reset seed by seed, keeping a seed
            // only when it is not reached already.
            std::set<uint32_t> reached;
            auto reach = [&](uint32_t from) {
                std::vector<uint32_t> w = keys_for(from, -1);
                while (!w.empty()) {
                    const uint32_t key = w.back();
                    w.pop_back();
                    auto it = insns.find(key);
                    if (it == insns.end() || !it->second.error.empty() || !reached.insert(key).second) continue;
                    const int v = int(key >> 24) - 1;
                    for (uint32_t t : it->second.succ)
                        for (uint32_t k : keys_for(t & 0xffffff, v)) w.push_back(k);
                    if (it->second.call_insn)
                        for (uint32_t k : keys_for(it->second.ret, v)) w.push_back(k);
                }
            };
            auto any = [&](uint32_t a, const auto &in) {
                for (uint32_t k : keys_for(a, -1))
                    if (in(k)) return true;
                return false;
            };
            reach(0xfffff0);
            std::ofstream o(reduce_path);
            o << "# " << set_name << ": V60 entry points MAME executed that the analysis does not reach (indirect\n"
              << "# branch and interrupt targets), from m1recomp --reduce on MAME's harvest (M1TRACE_PCS).\n"
              << "# Addresses only (rules.md). One hex address per line.\n";
            size_t kept = 0;
            for (uint32_t a : seeds)
                if (!any(a, [&](uint32_t k) { return reached.count(k) > 0; }) &&
                    any(a, [&](uint32_t k) { return insns.count(k) && insns[k].error.empty(); })) {
                    reach(a);
                    char b[16];
                    std::snprintf(b, sizeof b, "%06x\n", a);
                    o << b;
                    ++kept;
                }
            std::printf("m1recomp: %zu of %zu seeds kept in %s\n", kept, seeds.size(), reduce_path.c_str());
        }

        // Chunks of consecutive instructions, one function and one file each
        // (one function for the whole game takes the compiler too long): a
        // transfer inside the chunk is a goto, any other returns to run_native,
        // which finds the chunk for the new PC.
        std::vector<uint32_t> pcs; // keys, in order: the ROM's code, then each copy's
        for (auto &[key, in] : insns)
            if (in.error.empty()) pcs.push_back(key);
        auto lbl = [](uint32_t key) {
            return key >> 24 ? "c" + std::to_string((key >> 24) - 1) + "_" + hex6(key & 0xffffff) : hex6(key);
        };
        const std::string dir = argv[2];
        std::filesystem::create_directories(dir);
        for (const auto &e : std::filesystem::directory_iterator(dir))
            if (e.path().extension() == ".cpp") std::filesystem::remove(e.path());
        const char *kHeader = "// Generated by m1recomp from the user's ROM set: do not edit, never commit (rules.md 6).\n";
        std::vector<std::pair<uint32_t, uint32_t>> ranges;
        for (size_t start = 0, ci = 0; start < pcs.size(); ++ci) {
            // a chunk holds one copy's code (or the ROM's) only
            size_t end = start;
            while (end < pcs.size() && end - start < chunk && pcs[end] >> 24 == pcs[start] >> 24) ++end;
            const std::set<uint32_t> here(pcs.begin() + start, pcs.begin() + end);
            ranges.emplace_back(pcs[start], pcs[end - 1]);
            start = end;
            char name[32];
            std::snprintf(name, sizeof name, "chunk_%03zu", ci);
            std::ofstream o(dir + "/" + name + ".cpp");
            o << kHeader << "#include \"model1/native.h\"\n\nnamespace {\n\n";
            for (uint32_t key : here) {
                const Insn &in = insns[key];
                o << "struct H_" << lbl(key) << " : m1::NativeHooks {";
                for (auto &h : in.hooks) o << "\n    " << kHookSig.at(h.first) << " { " << h.second << "}";
                o << "\n};\n";
            }
            o << "\n} // namespace\n\n"
                 "namespace m1 {\n\n"
                 "// One instruction per label, in MAME's order: PC set (the handler reads\n"
                 "// it), the stop check, the handler with its resolved operands, PC\n"
                 "// advanced by its length, then the successor.\n"
                 "void " << name << "(v60::Core &c, NativeEnv &env) {\n"
                 "    dispatch:\n"
                 "    switch (c.m_reg[32] & 0xffffffu) {\n";
            for (uint32_t key : here) o << "    case 0x" << hex6(key & 0xffffff) << "u: goto L_" << lbl(key) << ";\n";
            o << "    default: return;\n    }\n";
            for (uint32_t key : here) {
                const Insn &in = insns[key];
                const uint32_t pc = key & 0xffffff;
                const int v = int(key >> 24) - 1;
                std::string call = in.call;
                call.replace(call.find("<H>"), 3, "<H_" + lbl(key) + ">");
                o << "L_" << lbl(key) << ": // " << in.name << "\n";
                o << "    c.m_reg[32] = (c.m_reg[32] & 0xff000000u) | 0x" << hex6(pc) << "u;\n";
                o << "    if (env.count >= env.limit) return;\n";
                o << "    " << in.prelude << "c.m_reg[32] += " << call << "; ++env.count;\n";
                // MAME's V60 takes a pending interrupt after the instruction
                // that lets it in (v60_try_irq after each one)
                if (in.name.rfind("opUPDPSW", 0) == 0 || in.name == "opLDPR" || in.name.rfind("opRETIU", 0) == 0 ||
                    in.name.rfind("opRETIS", 0) == 0)
                    o << "    if (env.irq_pending && c.interrupts_enabled()) return;\n";
                if (!in.indirect)
                    for (uint32_t s : in.succ) {
                        const uint32_t t = s & 0xffffff;
                        const std::vector<uint32_t> k = keys_for(t, v);
                        o << "    if ((c.m_reg[32] & 0xffffffu) == 0x" << hex6(t) << "u) "
                          << (k.size() == 1 && here.count(k[0]) ? "goto L_" + lbl(k[0]) + ";" : "return;") << "\n";
                    }
                // indirect, or a successor the analysis never produced
                o << "    goto dispatch;\n";
            }
            o << "}\n\n} // namespace m1\n";
        }
        {
            std::ofstream o(dir + "/table.cpp");
            o << kHeader << "#include \"model1/native.h\"\n\n#include <algorithm>\n#include <cstring>\n#include <iterator>\n\nnamespace m1 {\n\n";
            for (size_t i = 0; i < ranges.size(); i++) {
                char name[32];
                std::snprintf(name, sizeof name, "chunk_%03zu", i);
                o << "void " << name << "(v60::Core &, NativeEnv &);\n";
            }
            o << "\nnamespace {\nstruct Range {\n    uint32_t first, last;\n    void (*fn)(v60::Core &, NativeEnv &);\n};\n"
                 "const Range kRanges[] = {\n";
            for (size_t i = 0; i < ranges.size(); i++) {
                char b[96];
                std::snprintf(b, sizeof b, "    {0x%08xu, 0x%08xu, chunk_%03zu},\n", ranges[i].first, ranges[i].second, i);
                o << b;
            }
            o << "};\n";
            uint32_t lo = 0xffffffff, hi = 0;
            if (!copies.empty()) {
                // The code copied into RAM: the copy RAM holds now, by its bytes
                o << "struct Copy {\n    uint32_t from, to, bytes;\n};\nconst Copy kCopies[] = {\n";
                for (const m1::CodeCopy &c : copies) {
                    if ((c.from >> 16) != ((c.from + c.bytes - 1) >> 16) || (c.to >> 16) != ((c.to + c.bytes - 1) >> 16))
                        throw std::runtime_error("a code copy crosses a 64 KiB page");
                    char b[96];
                    std::snprintf(b, sizeof b, "    {0x%06xu, 0x%06xu, 0x%xu},\n", c.from, c.to, c.bytes);
                    o << b;
                    lo = std::min(lo, c.to);
                    hi = std::max(hi, c.to + c.bytes);
                }
                o << "};\n"
                     "uint32_t key_of(const v60::Core &c, uint32_t pc) {\n"
                     "    if (pc < 0x" << hex6(lo) << "u || pc >= 0x" << hex6(hi) << "u) return pc;\n"
                     "    for (uint32_t i = 0; i < std::size(kCopies); i++) {\n"
                     "        const Copy &k = kCopies[i];\n"
                     "        if (pc < k.to || pc >= k.to + k.bytes) continue;\n"
                     "        const uint8_t *ram = c.m_program->bus->rd[k.to >> 16], *rom = c.m_program->bus->rd[k.from >> 16];\n"
                     "        if (ram && rom && !std::memcmp(ram + (k.to & 0xffff), rom + (k.from & 0xffff), k.bytes)) return pc | (i + 1) << 24;\n"
                     "    }\n"
                     "    return pc; // none: no code\n"
                     "}\n";
            } else {
                o << "uint32_t key_of(const v60::Core &, uint32_t pc) { return pc; }\n";
            }
            // Every instruction's key, sorted: has_native (the fallback
            // interpreter runs until control is back at one of them)
            o << "const uint32_t kKeys[] = {\n";
            for (uint32_t key : pcs) {
                char b[32];
                std::snprintf(b, sizeof b, "    0x%08xu,\n", key);
                o << b;
            }
            o << "};\n";
            o << "} // namespace\n\n"
                 "bool has_native(const v60::Core &c, uint32_t pc) {\n"
                 "    return std::binary_search(std::begin(kKeys), std::end(kKeys), key_of(c, pc & 0xffffffu));\n"
                 "}\n\n"
                 "void run_native(v60::Core &c, NativeEnv &env) {\n"
                 "    while (env.count < env.limit && !(env.irq_pending && c.interrupts_enabled())) {\n"
                 "        const uint32_t key = key_of(c, c.m_reg[32] & 0xffffffu);\n"
                 "        const Range *r = std::upper_bound(std::begin(kRanges), std::end(kRanges), key,\n"
                 "                                          [](uint32_t a, const Range &x) { return a < x.first; });\n"
                 "        if (r == std::begin(kRanges) || key > (--r)->last) return env.miss(c);\n"
                 "        const uint64_t before = env.count;\n"
                 "        r->fn(c, env);\n"
                 "        // back with nothing run: no code at that address\n"
                 "        if (env.count == before && env.count < env.limit) return env.miss(c);\n"
                 "    }\n"
                 "}\n\n"
                 "uint64_t native_instructions() { return " << pcs.size() << "u; }\n\n} // namespace m1\n";
        }
        const size_t native = pcs.size();
        std::fprintf(stderr, "m1recomp: %zu instructions, %zu errors\n", native, errors.size());
        for (const auto &e : errors) std::fprintf(stderr, "  %s\n", e.c_str());
        return errors.empty() ? 0 : 1;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "m1recomp: %s\n", e.what());
        return 2;
    }
}
