// dsbrecomp: the Digital Sound Board's Z80 program (Star Wars Arcade's
// EPR-16471, Sega Touring Car Championship's EPR-19275) as C++, for
// src/model1/dsb. Every instruction reachable from reset, the interrupt
// vector (0x38) and the targets of the program's JP (HL) dispatch tables
// becomes straight-line code with its T-states counted (Zilog's timings, as
// MAME's Z80 runs them with no wait states); a return or an indirect jump
// goes through a switch over the instruction addresses. The flags are kept
// as the Z80 sets them (S, Z, H, P/V, N, C; bits 3 and 5 as MAME's do for
// the instructions decoded). Interrupts are taken between instructions, in
// interrupt mode 1, as MAME's Z80 takes them (not after EI until the next
// instruction). Only the instructions these programs use are decoded: an
// unknown opcode stops the recompile. The output is ROM-derived: keep it
// under build/, never commit it.
//
//   dsbrecomp DSB_CPU.bin OUT.cpp
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> rom;

uint8_t byte(uint32_t a) {
    if (a >= 0x8000 || a >= rom.size()) throw std::runtime_error("code outside the ROM");
    return rom[a];
}
uint16_t word(uint32_t a) { return uint16_t(byte(a) | byte(a + 1) << 8); }

std::string hex(unsigned v, int digits = 4) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%0*X", digits, v);
    return b;
}
std::string lbl(unsigned pc) { return "L_" + hex(pc).substr(2); }

struct Insn {
    unsigned len = 1;
    std::string code;              // the body (z.cycles already counts the instruction)
    unsigned cycles = 0;           // T-states (the not-taken count for conditionals)
    std::vector<uint16_t> next;    // successors the body may jump to
    bool falls = true;             // continues at pc + len
    std::vector<uint16_t> returns; // call: the return address is an entry too
    bool jp_hl = false;            // JP (HL): a dispatch table's jump
    int dev_at = -1;               // --io2: the T-state of its first access that may be a device's
};

const char *r8[8] = {"z.b", "z.c", "z.d", "z.e", "z.h", "z.l", "(hl)", "z.a"};

std::string get16(int p, const std::string &idx = "") {
    switch (p) {
    case 0: return "uint16_t(z.b << 8 | z.c)";
    case 1: return "uint16_t(z.d << 8 | z.e)";
    case 2: return idx.empty() ? "uint16_t(z.h << 8 | z.l)" : "z." + idx;
    default: return "z.sp";
    }
}
std::string set16(int p, const std::string &v, const std::string &idx = "") {
    switch (p) {
    case 0: return "{ const uint16_t v_ = " + v + "; z.b = uint8_t(v_ >> 8), z.c = uint8_t(v_); }";
    case 1: return "{ const uint16_t v_ = " + v + "; z.d = uint8_t(v_ >> 8), z.e = uint8_t(v_); }";
    case 2:
        if (!idx.empty()) return "z." + idx + " = " + v + ";";
        return "{ const uint16_t v_ = " + v + "; z.h = uint8_t(v_ >> 8), z.l = uint8_t(v_); }";
    default: return "z.sp = " + v + ";";
    }
}
// push/pop register pairs: BC, DE, HL (or IX/IY), AF
std::string get16af(int p, const std::string &idx) { return p == 3 ? "uint16_t(z.a << 8 | z.f)" : get16(p, idx); }
std::string set16af(int p, const std::string &v, const std::string &idx) {
    if (p == 3) return "{ const uint16_t v_ = " + v + "; z.a = uint8_t(v_ >> 8), z.f = uint8_t(v_); }";
    return set16(p, v, idx);
}
const std::string HL = "uint16_t(z.h << 8 | z.l)";

// --io2: Model 1's model1io2 I/O board BIOS (EPR-16891). Its Z80 reaches
// the board's devices through memory at 8000-DFFF, so each memory or I/O
// access names the T-state it happens on within the instruction (MAME's
// z80.lst: each M-cycle's access comes at its start), and an instruction
// that may touch a device does not start while that T-state is at or past
// the run's end (the device sees it in a later timeslice).
bool io2 = false;
int dev_at = -1; // the instruction being decoded: its first access that may be a device's

std::string at(int off) { return off < 0 ? "-1" : std::to_string(off); }
// A memory access at T-state `off`; `ram`: known to be RAM or ROM (the stack)
bool device_address(const std::string &a) {
    if (a.rfind("0x", 0) != 0) return true; // computed
    const unsigned v = unsigned(std::stoul(a, nullptr, 16));
    return v >= 0x8000 && v < 0xe000;
}
void note(const std::string &a, int off) {
    if (io2 && off >= 0 && device_address(a) && (dev_at < 0 || off < dev_at)) dev_at = off;
}
std::string R(const std::string &a, int off) {
    note(a, off);
    return io2 ? "rd(z, " + a + ", " + at(off) + ")" : "rd(z, " + a + ")";
}
std::string W(const std::string &a, const std::string &v, int off) {
    note(a, off);
    return io2 ? "wr(z, " + a + ", " + v + ", " + at(off) + ")" : "wr(z, " + a + ", " + v + ")";
}
std::string R16(const std::string &a, int off) {
    note(a, off);
    return io2 ? "rd16(z, " + a + ", " + at(off) + ")" : "rd16(z, " + a + ")";
}
std::string W16(const std::string &a, const std::string &v, int off) {
    note(a, off);
    return io2 ? "wr16(z, " + a + ", " + v + ", " + at(off) + ")" : "wr16(z, " + a + ", " + v + ")";
}
// The stack (RAM)
std::string RS(const std::string &a) { return io2 ? "rd16(z, " + a + ", -1)" : "rd16(z, " + a + ")"; }
std::string WS(const std::string &a, const std::string &v) { return io2 ? "wr16(z, " + a + ", " + v + ", -1)" : "wr16(z, " + a + ", " + v + ")"; }
// The I/O space at T-state `off` (the DSB's board takes the instruction's end)
std::string when(int off) { return io2 ? "z.start + " + std::to_string(off) : "z.cycles"; }
std::string IN(const std::string &port, int off) { return "z.io->in(" + port + ", " + when(off) + ")"; }
std::string OUT(const std::string &port, const std::string &v, int off) {
    return "z.io->out(" + port + ", " + v + ", " + when(off) + ")";
}

std::string cond(int cc) {
    static const char *c[8] = {"!(z.f & ZF)", "(z.f & ZF)", "!(z.f & CF)", "(z.f & CF)",
                               "!(z.f & PF)", "(z.f & PF)", "!(z.f & SF)", "(z.f & SF)"};
    return c[cc];
}

// ALU operation y on A with value v (an expression).
std::string alu(int y, const std::string &v) {
    switch (y) {
    case 0: return "add8(z, " + v + ", 0);";
    case 1: return "add8(z, " + v + ", z.f & CF);";
    case 2: return "z.a = sub8(z, " + v + ", 0);";
    case 3: return "z.a = sub8(z, " + v + ", z.f & CF);";
    case 4: return "z.a &= " + v + "; z.f = uint8_t(szp(z.a) | HF);";
    case 5: return "z.a ^= " + v + "; z.f = szp(z.a);";
    case 6: return "z.a |= " + v + "; z.f = szp(z.a);";
    default: return "cp8(z, " + v + ");";
    }
}

// DD/FD: IX or IY in place of HL; (HL) operands become (IX+d).
Insn decode_indexed(uint16_t pc, const std::string &idx) {
    Insn i;
    const uint8_t op = byte(pc + 1);
    const int x = op >> 6, y = (op >> 3) & 7, zz = op & 7, p = y >> 1;
    auto ea = [&] { return "uint16_t(z." + idx + " + " + std::to_string(int8_t(byte(pc + 2))) + ")"; };
    const std::string I = "z." + idx;
    if (op == 0x21) return i.len = 4, i.cycles = 14, i.code = I + " = " + hex(word(pc + 2)) + ";", i;
    if (op == 0x22) return i.len = 4, i.cycles = 20, i.code = W16(hex(word(pc + 2)), I, 14) + ";", i;
    if (op == 0x2A) return i.len = 4, i.cycles = 20, i.code = I + " = " + R16(hex(word(pc + 2)), 14) + ";", i;
    if (op == 0x23) return i.len = 2, i.cycles = 10, i.code = I + " = uint16_t(" + I + " + 1);", i;
    if (op == 0x2B) return i.len = 2, i.cycles = 10, i.code = I + " = uint16_t(" + I + " - 1);", i;
    if (op == 0xE1) return i.len = 2, i.cycles = 14, i.code = I + " = " + RS("z.sp") + "; z.sp = uint16_t(z.sp + 2);", i;
    if (op == 0xE5) return i.len = 2, i.cycles = 15, i.code = "z.sp = uint16_t(z.sp - 2); " + WS("z.sp", I) + ";", i;
    if (op == 0xE9) return i.len = 2, i.cycles = 8, i.falls = false, i.code = "z.pc = " + I + "; goto dispatch;", i;
    if (x == 0 && zz == 1 && (y & 1)) // add ix,rr
        return i.len = 2, i.cycles = 15, i.code = I + " = add16(z, " + I + ", " + get16(p, idx) + ");", i;
    if (op == 0x34 || op == 0x35) {
        i.len = 3, i.cycles = 23;
        i.code = "{ const uint16_t a_ = " + ea() + "; " + W("a_", std::string(op == 0x34 ? "inc8" : "dec8") + "(z, " + R("a_", 16) + ")", 20) + "; }";
        return i;
    }
    if (op == 0x36) return i.len = 4, i.cycles = 19, i.code = W(ea(), hex(byte(pc + 3), 2), 16) + ";", i;
    if (op == 0xCB && (byte(pc + 3) & 7) == 6) { // DD CB d op: rotate, bit, res, set (IX+d)
        const uint8_t o = byte(pc + 3);
        const int b = (o >> 3) & 7, g = o >> 6;
        i.len = 4;
        std::string body;
        if (g == 0) {
            static const char *fn[8] = {"rlc", "rrc", "rl", "rr", "sla", "sra", "sll", "srl"};
            body = std::string("v_ = ") + fn[b] + "(z, v_);";
        } else if (g == 1) { // bits 3 and 5 from the address's high byte (MAME bit_xy)
            body = "bit(z, " + std::to_string(b) + ", v_); z.f = uint8_t((z.f & ~(YF | XF)) | ((a_ >> 8) & (YF | XF)));";
        } else {
            body = std::string("v_") + (g == 2 ? " &= uint8_t(~" : " |= uint8_t(") + hex(1u << b, 2) + ");";
        }
        i.cycles = g == 1 ? 20 : 23;
        i.code = "{ const uint16_t a_ = " + ea() + "; uint8_t v_ = " + R("a_", 16) + "; " + body +
                 (g == 1 ? "" : " " + W("a_", "v_", 20) + ";") + " }";
        return i;
    }
    if (x == 1 && zz == 6 && y != 6) return i.len = 3, i.cycles = 19, i.code = std::string(r8[y]) + " = " + R(ea(), 16) + ";", i;
    if (x == 1 && y == 6 && zz != 6) return i.len = 3, i.cycles = 19, i.code = W(ea(), r8[zz], 16) + ";", i;
    if (x == 2 && zz == 6) return i.len = 3, i.cycles = 19, i.code = alu(y, R(ea(), 16)), i;
    char b[64];
    std::snprintf(b, sizeof b, "opcode %s %02X not decoded at %04X", idx.c_str(), op, pc);
    throw std::runtime_error(b);
}

Insn decode(uint16_t pc) {
    Insn i;
    const uint8_t op = byte(pc);
    const int x = op >> 6, y = (op >> 3) & 7, zz = op & 7, p = y >> 1;
    auto n8 = [&] { return hex(byte(pc + 1), 2); };
    auto n16 = [&] { return hex(word(pc + 1)); };
    auto reg8 = [&](int r) { return r == 6 ? R(HL, 4) : std::string(r8[r]); };
    if (op == 0xDD) return decode_indexed(pc, "ix");
    if (op == 0xFD) return decode_indexed(pc, "iy");
    if (op == 0x00) return i.cycles = 4, i;                                               // nop
    if (op == 0xF3) return i.cycles = 4, i.code = "z.iff1 = z.iff2 = false;", i;          // di
    if (op == 0xFB) return i.cycles = 4, i.code = "z.iff1 = z.iff2 = true; z.after_ei = true;", i; // ei
    if (op == 0x76) throw std::runtime_error("halt not decoded");
    if (x == 0 && zz == 1 && !(y & 1)) return i.len = 3, i.cycles = 10, i.code = set16(p, n16()), i; // ld rr,nn
    if (x == 0 && zz == 1 && (y & 1)) return i.cycles = 11, i.code = set16(2, "add16(z, " + HL + ", " + get16(p) + ")"), i;
    if (x == 0 && zz == 3) return i.cycles = 6, i.code = set16(p, "uint16_t(" + get16(p) + (y & 1 ? " - 1)" : " + 1)")), i;
    if (x == 0 && (zz == 4 || zz == 5)) {                                                 // inc/dec r
        const std::string fn = zz == 4 ? "inc8" : "dec8";
        if (y == 6) i.cycles = 11, i.code = W(HL, fn + "(z, " + R(HL, 4) + ")", 8) + ";";
        else i.cycles = 4, i.code = std::string(r8[y]) + " = " + fn + "(z, " + r8[y] + ");";
        return i;
    }
    if (x == 0 && zz == 6) {                                                              // ld r,n
        i.len = 2;
        if (y == 6) i.cycles = 10, i.code = W(HL, n8(), 7) + ";";
        else i.cycles = 7, i.code = std::string(r8[y]) + " = " + n8() + ";";
        return i;
    }
    if (x == 0 && zz == 0 && y >= 3) {                                                    // jr / jr cc
        const uint16_t t = uint16_t(pc + 2 + int8_t(byte(pc + 1)));
        i.len = 2, i.next = {t};
        if (y == 3) return i.cycles = 12, i.falls = false, i.code = "goto " + lbl(t) + ";", i;
        i.cycles = 7, i.code = "if (" + cond(y - 4) + ") { z.cycles += 5; goto " + lbl(t) + "; }";
        return i;
    }
    switch (op) {
    case 0x02: i.cycles = 7, i.code = W(get16(0), "z.a", 4) + ";"; return i;
    case 0x12: i.cycles = 7, i.code = W(get16(1), "z.a", 4) + ";"; return i;
    case 0x0A: i.cycles = 7, i.code = "z.a = " + R(get16(0), 4) + ";"; return i;
    case 0x1A: i.cycles = 7, i.code = "z.a = " + R(get16(1), 4) + ";"; return i;
    case 0x07: i.cycles = 4, i.code = "z.a = uint8_t(z.a << 1 | z.a >> 7); z.f = uint8_t((z.f & (SF | ZF | PF)) | (z.a & (YF | XF | CF)));"; return i; // rlca
    case 0x0F: i.cycles = 4, i.code = "z.f = uint8_t((z.f & (SF | ZF | PF)) | (z.a & CF)); z.a = uint8_t(z.a >> 1 | z.a << 7); z.f |= z.a & (YF | XF);"; return i; // rrca
    case 0x17: i.cycles = 4, i.code = "{ const uint8_t c_ = z.a >> 7; z.a = uint8_t(z.a << 1 | (z.f & CF)); z.f = uint8_t((z.f & (SF | ZF | PF)) | (z.a & (YF | XF)) | c_); }"; return i; // rla
    case 0x1F: i.cycles = 4, i.code = "{ const uint8_t c_ = z.a & 1; z.a = uint8_t(z.a >> 1 | (z.f & CF) << 7); z.f = uint8_t((z.f & (SF | ZF | PF)) | (z.a & (YF | XF)) | c_); }"; return i; // rra
    case 0x2F: i.cycles = 4, i.code = "z.a = uint8_t(~z.a); z.f = uint8_t((z.f & (SF | ZF | PF | CF)) | HF | NF | (z.a & (YF | XF)));"; return i; // cpl
    case 0x37: i.cycles = 4, i.code = "z.f = uint8_t((z.f & (SF | ZF | PF)) | (z.a & (YF | XF)) | CF);"; return i; // scf
    case 0x3F: i.cycles = 4, i.code = "z.f = uint8_t(((z.f & (SF | ZF | PF | CF)) | ((z.f & CF) << 4) | (z.a & (YF | XF))) ^ CF);"; return i; // ccf
    case 0x08: i.cycles = 4, i.code = "std::swap(z.a, z.a2); std::swap(z.f, z.f2);"; return i; // ex af,af'
    case 0xD9: i.cycles = 4, i.code = "std::swap(z.b, z.b2); std::swap(z.c, z.c2); std::swap(z.d, z.d2); std::swap(z.e, z.e2); std::swap(z.h, z.h2); std::swap(z.l, z.l2);"; return i; // exx
    case 0xEB: i.cycles = 4, i.code = "std::swap(z.d, z.h); std::swap(z.e, z.l);"; return i; // ex de,hl
    case 0x10: { // djnz
        const uint16_t t = uint16_t(pc + 2 + int8_t(byte(pc + 1)));
        i.len = 2, i.cycles = 8, i.next = {t};
        i.code = "z.b = uint8_t(z.b - 1); if (z.b) { z.cycles += 5; goto " + lbl(t) + "; }";
        return i;
    }
    case 0x22: i.len = 3, i.cycles = 16, i.code = W16(n16(), HL, 10) + ";"; return i;
    case 0x2A: i.len = 3, i.cycles = 16, i.code = set16(2, R16(n16(), 10)); return i;
    case 0x32: i.len = 3, i.cycles = 13, i.code = W(n16(), "z.a", 10) + ";"; return i;
    case 0x3A: i.len = 3, i.cycles = 13, i.code = "z.a = " + R(n16(), 10) + ";"; return i;
    case 0xC3: { // jp nn
        i.len = 3, i.cycles = 10, i.falls = false, i.next = {word(pc + 1)};
        i.code = "goto " + lbl(word(pc + 1)) + ";";
        return i;
    }
    case 0xC9: // ret
        i.cycles = 10, i.falls = false;
        i.code = "z.pc = " + RS("z.sp") + "; z.sp = uint16_t(z.sp + 2); goto dispatch;";
        return i;
    case 0xCD: { // call nn
        const uint16_t t = word(pc + 1), r = uint16_t(pc + 3);
        i.len = 3, i.cycles = 17, i.falls = false, i.next = {t}, i.returns = {r};
        i.code = "z.sp = uint16_t(z.sp - 2); " + WS("z.sp", hex(r)) + "; goto " + lbl(t) + ";";
        return i;
    }
    case 0xD3: i.len = 2, i.cycles = 11, i.code = OUT(n8(), "z.a", 7) + ";"; return i; // out (n),a
    case 0xDB: i.len = 2, i.cycles = 11, i.code = "z.a = " + IN(n8(), 7) + ";"; return i; // in a,(n)
    case 0xE3: // ex (sp),hl
        i.cycles = 19;
        i.code = "{ const uint16_t t = " + RS("z.sp") + "; " + WS("z.sp", HL) + "; " + set16(2, "t") + " }";
        return i;
    case 0xE9: i.cycles = 4, i.falls = false, i.jp_hl = true, i.code = "z.pc = " + HL + "; goto dispatch;"; return i; // jp (hl)
    case 0xF9: i.cycles = 6, i.code = "z.sp = " + HL + ";"; return i;
    case 0xCB: {
        const uint8_t o = byte(pc + 1);
        const int r = o & 7, b = (o >> 3) & 7, g = o >> 6;
        i.len = 2;
        const bool mem = r == 6;
        const std::string R_ = mem ? "v_" : r8[r];
        std::string body;
        if (g == 0) {
            static const char *fn[8] = {"rlc", "rrc", "rl", "rr", "sla", "sra", "sll", "srl"};
            body = R_ + " = " + fn[b] + "(z, " + R_ + ");";
        } else if (g == 1) {
            body = "bit(z, " + std::to_string(b) + ", " + R_ + ");";
        } else {
            body = R_ + std::string(g == 2 ? " &= uint8_t(~" : " |= uint8_t(") + hex(1u << b, 2) + ");";
        }
        if (!mem) return i.cycles = 8, i.code = body, i;
        i.cycles = g == 1 ? 12 : 15;
        i.code = "{ uint8_t v_ = " + R(HL, 8) + "; " + body + (g == 1 ? "" : " " + W(HL, "v_", 12) + ";") + " }";
        return i;
    }
    case 0xED: {
        const uint8_t o = byte(pc + 1);
        const int ey = (o >> 3) & 7, ep = ey >> 1;
        i.len = 2;
        if (o == 0x46) return i.cycles = 8, i.code = "z.im = 0;", i;
        if (o == 0x56) return i.cycles = 8, i.code = "z.im = 1;", i;
        if (o == 0x5E) return i.cycles = 8, i.code = "z.im = 2;", i;
        if (o == 0x47) return i.cycles = 9, i.code = "z.i = z.a;", i; // ld i,a
        if (o == 0x4D || o == 0x45) { // reti, retn: IFF1 from IFF2
            i.cycles = 14, i.falls = false;
            i.code = std::string(io2 && o == 0x4D ? "z.io->reti(z.start + 14); " : "") +
                     "z.iff1 = z.iff2; z.pc = " + RS("z.sp") + "; z.sp = uint16_t(z.sp + 2); goto dispatch;";
            return i;
        }
        if ((o & 0xC7) == 0x41 && ey != 6) return i.cycles = 12, i.code = OUT("z.c", r8[ey], 8) + ";", i;
        if ((o & 0xC7) == 0x40 && ey != 6) return i.cycles = 12, i.code = std::string(r8[ey]) + " = " + IN("z.c", 8) + "; z.f = uint8_t((z.f & CF) | szp(" + r8[ey] + "));", i;
        if ((o & 0xCF) == 0x43) return i.len = 4, i.cycles = 20, i.code = W16(hex(word(pc + 2)), get16(ep), 14) + ";", i;
        if ((o & 0xCF) == 0x4B) return i.len = 4, i.cycles = 20, i.code = set16(ep, R16(hex(word(pc + 2)), 14)), i;
        if ((o & 0xCF) == 0x42) return i.cycles = 15, i.code = set16(2, "sbc16(z, " + HL + ", " + get16(ep) + ")"), i;
        if ((o & 0xCF) == 0x4A) return i.cycles = 15, i.code = set16(2, "adc16(z, " + HL + ", " + get16(ep) + ")"), i;
        if (o == 0x44) return i.cycles = 8, i.code = "{ const uint8_t v_ = z.a; z.a = 0; z.a = sub8(z, v_, 0); }", i; // neg
        if (o == 0xA3) {                                                                   // outi
            i.cycles = 16;
            i.code = "{ const uint8_t v = " + R(HL, 9) + "; z.b = uint8_t(z.b - 1); " + OUT("z.c", "v", 12) + "; " +
                     set16(2, "uint16_t(" + HL + " + 1)") + " io_flags(z, v, z.l); }";
            return i;
        }
        if (o == 0xB3 && io2) { // otir: outi until B is 0 (21 T-states each but the last), each out 12 into its pass
            i.cycles = 0;
            i.code = "for (;;) { z.start = z.cycles; const uint8_t v = " + R(HL, 9) + "; z.b = uint8_t(z.b - 1); " +
                     OUT("z.c", "v", 12) + "; " + set16(2, "uint16_t(" + HL + " + 1)") +
                     " io_flags(z, v, z.l); if (!z.b) { z.cycles += 16; break; } z.cycles += 21; }";
            return i;
        }
        if (o == 0xB0) {                                                                   // ldir
            i.cycles = 0;
            i.code = "for (;;) { const uint8_t v_ = " + R(HL, -1) + "; " + W(get16(1), "v_", -1) + "; " +
                     set16(1, "uint16_t(" + get16(1) + " + 1)") + " " + set16(2, "uint16_t(" + HL + " + 1)") + " " +
                     set16(0, "uint16_t(" + get16(0) + " - 1)") + " ld_flags(z, v_); if (!(z.b | z.c)) { z.cycles += 16; break; } z.cycles += 21; }";
            return i;
        }
        throw std::runtime_error("ED opcode not decoded at " + hex(pc));
    }
    }
    if (x == 1) { // ld r,r'
        i.cycles = (y == 6 || zz == 6) ? 7 : 4;
        i.code = y == 6 ? W(HL, reg8(zz), 4) + ";" : std::string(r8[y]) + " = " + reg8(zz) + ";";
        return i;
    }
    if (x == 2) return i.cycles = zz == 6 ? 7 : 4, i.code = alu(y, reg8(zz)), i; // alu r
    if (x == 3 && zz == 6) return i.len = 2, i.cycles = 7, i.code = alu(y, n8()), i; // alu n
    if (x == 3 && zz == 0) { // ret cc
        i.cycles = 5;
        i.code = "if (" + cond(y) + ") { z.cycles += 6; z.pc = " + RS("z.sp") + "; z.sp = uint16_t(z.sp + 2); goto dispatch; }";
        return i;
    }
    if (x == 3 && zz == 1 && !(y & 1)) // pop rr
        return i.cycles = 10, i.code = set16af(p, RS("z.sp"), "") + " z.sp = uint16_t(z.sp + 2);", i;
    if (x == 3 && zz == 5 && !(y & 1)) // push rr
        return i.cycles = 11, i.code = "z.sp = uint16_t(z.sp - 2); " + WS("z.sp", get16af(p, "")) + ";", i;
    if (x == 3 && zz == 2) { // jp cc,nn
        const uint16_t t = word(pc + 1);
        i.len = 3, i.cycles = 10, i.next = {t};
        i.code = "if (" + cond(y) + ") goto " + lbl(t) + ";";
        return i;
    }
    if (x == 3 && zz == 4) { // call cc,nn
        const uint16_t t = word(pc + 1), r = uint16_t(pc + 3);
        i.len = 3, i.cycles = 10, i.next = {t}, i.returns = {r};
        i.code = "if (" + cond(y) + ") { z.cycles += 7; z.sp = uint16_t(z.sp - 2); " + WS("z.sp", hex(r)) + "; goto " + lbl(t) + "; }";
        return i;
    }
    if (x == 3 && zz == 7) { // rst
        const uint16_t t = uint16_t(y * 8), r = uint16_t(pc + 1);
        i.cycles = 11, i.falls = false, i.next = {t}, i.returns = {r};
        i.code = "z.sp = uint16_t(z.sp - 2); " + WS("z.sp", hex(r)) + "; goto " + lbl(t) + ";";
        return i;
    }
    char b[64];
    std::snprintf(b, sizeof b, "opcode %02X not decoded at %04X", op, pc);
    throw std::runtime_error(b);
}

// A JP (HL) dispatch table: LD HL,table; ADD HL,BC; PUSH DE; LD E,(HL);
// INC HL; LD D,(HL); EX DE,HL; POP DE; JP (HL). Returns the table address.
int dispatch_table(uint16_t jp) {
    static const uint8_t tail[] = {0x09, 0xD5, 0x5E, 0x23, 0x56, 0xEB, 0xD1, 0xE9};
    if (jp < 10) return -1;
    for (int k = 0; k < 8; k++)
        if (rom[jp - 7 + k] != tail[k]) return -1;
    if (rom[jp - 10] != 0x21) return -1;
    return rom[jp - 9] | rom[jp - 8] << 8;
}

// The I/O board BIOS's tables: AND mask; ADD A,A; LD D,0; LD E,A;
// LD IX,table; ADD IX,DE; LD L,(IX+0); LD H,(IX+1); JP (HL). The table and
// its entries (mask + 1), or -1.
int indexed_table(uint16_t jp, int &entries) {
    static const uint8_t pat[] = {0xE6, 0, 0x87, 0x16, 0x00, 0x5F, 0xDD, 0x21, 0, 0, 0xDD, 0x19,
                                  0xDD, 0x6E, 0x00, 0xDD, 0x66, 0x01, 0xE9};
    if (jp < 18) return -1;
    const unsigned b = jp - 18u;
    for (unsigned k = 0; k < sizeof pat; k++)
        if (k != 1 && k != 8 && k != 9 && rom[b + k] != pat[k]) return -1;
    entries = rom[b + 1] + 1;
    return rom[b + 8] | rom[b + 9] << 8;
}

const char *kPreamble = R"(namespace {
constexpr uint8_t SF = 0x80, ZF = 0x40, YF = 0x20, HF = 0x10, XF = 0x08, PF = 0x04, NF = 0x02, CF = 0x01;
inline uint8_t rd(Z80 &z, uint16_t a) { return z.mem[a]; }
inline void wr(Z80 &z, uint16_t a, uint8_t v) { if (a >= 0x8000) z.mem[a] = v; }
inline uint16_t rd16(Z80 &z, uint16_t a) { return uint16_t(rd(z, a) | rd(z, uint16_t(a + 1)) << 8); }
inline void wr16(Z80 &z, uint16_t a, uint16_t v) { wr(z, a, uint8_t(v)); wr(z, uint16_t(a + 1), uint8_t(v >> 8)); }
inline uint8_t szp(uint8_t v) { return uint8_t((v & (SF | YF | XF)) | (v ? 0 : ZF) | ((std::popcount(v) & 1) ? 0 : PF)); }
inline uint8_t sz(uint8_t v) { return uint8_t((v & (SF | YF | XF)) | (v ? 0 : ZF)); }
inline void add8(Z80 &z, uint8_t v, unsigned c) {
    const unsigned r = z.a + v + c;
    z.f = uint8_t(sz(uint8_t(r)) | ((z.a ^ v ^ r) & HF) | (((z.a ^ ~v) & (z.a ^ r) & 0x80) >> 5) | (r >> 8));
    z.a = uint8_t(r);
}
inline uint8_t sub8(Z80 &z, uint8_t v, unsigned c) {
    const unsigned r = unsigned(z.a) - v - c;
    z.f = uint8_t(sz(uint8_t(r)) | ((z.a ^ v ^ r) & HF) | (((z.a ^ v) & (z.a ^ r) & 0x80) >> 5) | NF | ((r >> 8) & CF));
    return uint8_t(r);
}
inline void cp8(Z80 &z, uint8_t v) { // bits 3 and 5 from the operand
    sub8(z, v, 0);
    z.f = uint8_t((z.f & ~(YF | XF)) | (v & (YF | XF)));
}
inline uint8_t inc8(Z80 &z, uint8_t v) {
    const uint8_t r = uint8_t(v + 1);
    z.f = uint8_t((z.f & CF) | sz(r) | ((r & 0x0f) ? 0 : HF) | (r == 0x80 ? PF : 0));
    return r;
}
inline uint8_t dec8(Z80 &z, uint8_t v) {
    const uint8_t r = uint8_t(v - 1);
    z.f = uint8_t((z.f & CF) | NF | sz(r) | ((r & 0x0f) == 0x0f ? HF : 0) | (r == 0x7f ? PF : 0));
    return r;
}
inline uint16_t add16(Z80 &z, uint16_t a, uint16_t v) {
    const uint32_t r = uint32_t(a) + v;
    z.f = uint8_t((z.f & (SF | ZF | PF)) | ((r >> 8) & (YF | XF)) | (((a ^ v ^ r) >> 8) & HF) | (r >> 16));
    return uint16_t(r);
}
inline uint16_t adc16(Z80 &z, uint16_t a, uint16_t v) {
    const uint32_t r = uint32_t(a) + v + (z.f & CF);
    z.f = uint8_t(((r >> 8) & (SF | YF | XF)) | ((r & 0xffff) ? 0 : ZF) | (((a ^ v ^ r) >> 8) & HF) |
                  (((a ^ ~v) & (a ^ r) & 0x8000) >> 13) | (r >> 16));
    return uint16_t(r);
}
inline uint16_t sbc16(Z80 &z, uint16_t a, uint16_t v) {
    const uint32_t r = uint32_t(a) - v - (z.f & CF);
    z.f = uint8_t(((r >> 8) & (SF | YF | XF)) | ((r & 0xffff) ? 0 : ZF) | (((a ^ v ^ r) >> 8) & HF) |
                  (((a ^ v) & (a ^ r) & 0x8000) >> 13) | NF | ((r >> 16) & CF));
    return uint16_t(r);
}
inline uint8_t rlc(Z80 &z, uint8_t v) { const uint8_t r = uint8_t(v << 1 | v >> 7); z.f = uint8_t(szp(r) | (v >> 7)); return r; }
inline uint8_t rrc(Z80 &z, uint8_t v) { const uint8_t r = uint8_t(v >> 1 | v << 7); z.f = uint8_t(szp(r) | (v & 1)); return r; }
inline uint8_t rl(Z80 &z, uint8_t v) { const uint8_t r = uint8_t(v << 1 | (z.f & CF)); z.f = uint8_t(szp(r) | (v >> 7)); return r; }
inline uint8_t rr(Z80 &z, uint8_t v) { const uint8_t r = uint8_t(v >> 1 | (z.f & CF) << 7); z.f = uint8_t(szp(r) | (v & 1)); return r; }
inline uint8_t sla(Z80 &z, uint8_t v) { const uint8_t r = uint8_t(v << 1); z.f = uint8_t(szp(r) | (v >> 7)); return r; }
inline uint8_t sra(Z80 &z, uint8_t v) { const uint8_t r = uint8_t((v >> 1) | (v & 0x80)); z.f = uint8_t(szp(r) | (v & 1)); return r; }
inline uint8_t sll(Z80 &z, uint8_t v) { const uint8_t r = uint8_t(v << 1 | 1); z.f = uint8_t(szp(r) | (v >> 7)); return r; }
inline uint8_t srl(Z80 &z, uint8_t v) { const uint8_t r = uint8_t(v >> 1); z.f = uint8_t(szp(r) | (v & 1)); return r; }
inline void bit(Z80 &z, int n, uint8_t v) {
    z.f = uint8_t((z.f & CF) | HF | (v & (YF | XF)) | ((v >> n) & 1 ? 0 : (ZF | PF)) | (n == 7 && (v & 0x80) ? SF : 0));
}
inline void ld_flags(Z80 &z, uint8_t v) { // LDI/LDIR: P/V while BC is not 0
    const uint8_t n = uint8_t(v + z.a);
    z.f = uint8_t((z.f & (SF | ZF | CF)) | ((z.b | z.c) ? PF : 0) | (n & XF) | ((n << 4) & YF));
}
inline void io_flags(Z80 &z, uint8_t v, uint8_t l) { // OUTI (after B is decremented and HL incremented)
    const unsigned t = unsigned(v) + l;
    z.f = uint8_t(sz(z.b) | ((v & 0x80) ? NF : 0) | (t > 0xff ? (HF | CF) : 0) | ((std::popcount(uint8_t((t & 7) ^ z.b)) & 1) ? 0 : PF));
}
// MAME take_interrupt in IM 1: RST 38h, 13 T-states.
inline void take_irq(Z80 &z, uint16_t pc) {
    z.iff1 = z.iff2 = false;
    z.cycles += 13;
    z.sp = uint16_t(z.sp - 2);
    wr16(z, z.sp, pc);
}
} // namespace

)";

// --io2: memory through the board (ROM 0000-7FFF and RAM E000-FFFF here,
// the rest the board's devices, at the access's T-state); IM 2 interrupts.
const char *kPreambleIo2 = R"(namespace {
constexpr uint8_t SF = 0x80, ZF = 0x40, YF = 0x20, HF = 0x10, XF = 0x08, PF = 0x04, NF = 0x02, CF = 0x01;
inline uint8_t rd(Z80 &z, uint16_t a, int off) {
    if (a < 0x8000 || a >= 0xe000) return z.mem[a];
    if (off < 0) throw std::runtime_error("I/O board Z80: untimed device read at " + std::to_string(a));
    return z.io->mem_r(a, z.start + uint64_t(off));
}
inline void wr(Z80 &z, uint16_t a, uint8_t v, int off) {
    if (a >= 0xe000) z.mem[a] = v;
    else if (a >= 0x8000) {
        if (off < 0) throw std::runtime_error("I/O board Z80: untimed device write at " + std::to_string(a));
        z.io->mem_w(a, v, z.start + uint64_t(off));
    }
}
inline uint16_t rd16(Z80 &z, uint16_t a, int off) { return uint16_t(rd(z, a, off) | rd(z, uint16_t(a + 1), off < 0 ? off : off + 3) << 8); }
inline void wr16(Z80 &z, uint16_t a, uint16_t v, int off) { wr(z, a, uint8_t(v), off); wr(z, uint16_t(a + 1), uint8_t(v >> 8), off < 0 ? off : off + 3); }
)";

// The flag helpers, shared by both (kPreamble from its first helper to its IM 1 take_irq)
std::string helpers() {
    const std::string p = kPreamble;
    const size_t a = p.find("inline uint8_t szp"), b = p.find("// MAME take_interrupt in IM 1");
    return p.substr(a, b - a);
}

// MAME take_interrupt in IM 2: the vector from the daisy chain, 2 T-states'
// latency, a call's 5, the push and the table read: 19 T-states.
const char *kTakeIrqIo2 = R"(inline void take_irq(Z80 &z, uint16_t pc) {
    z.iff1 = z.iff2 = false;
    const uint8_t v = z.io->irq_ack(z.cycles);
    z.cycles += 7;
    z.sp = uint16_t(z.sp - 2);
    wr16(z, z.sp, pc, -1);
    z.cycles += 6;
    z.pc = rd16(z, uint16_t(z.i << 8 | v), -1);
    z.cycles += 6;
}
} // namespace

)";

} // namespace

int main(int argc, char **argv) {
    if (argc == 4 && std::string(argv[1]) == "--io2") {
        io2 = true;
        ++argv, --argc;
    }
    if (argc != 3) {
        std::fprintf(stderr, "usage: dsbrecomp [--io2] PROGRAM.bin OUT.cpp\n");
        return 2;
    }
    try {
        std::ifstream f(argv[1], std::ios::binary);
        rom.assign(std::istreambuf_iterator<char>(f), {});
        if (rom.size() < 0x8000) throw std::runtime_error("the DSB program image is too small");

        std::map<uint16_t, Insn> code;
        std::set<int> tables;
        std::set<uint16_t> outside;
        std::vector<uint16_t> todo = {0x0000, 0x0038};
        if (io2) { // reset and the CTC's IM 2 vectors (the BIOS's table at 00E8)
            todo = {0x0000};
            for (unsigned v = 0xe8; v < 0xf0; v += 2)
                if (word(v) < 0x8000) todo.push_back(word(v));
        }
        for (;;) {
            while (!todo.empty()) {
                const uint16_t pc = todo.back();
                todo.pop_back();
                if (code.count(pc)) continue;
                if (io2 && pc >= 0x8000) { // the BIOS's debug mode (8400): no code here
                    outside.insert(pc);
                    continue;
                }
                dev_at = -1;
                Insn i;
                try {
                    i = decode(pc);
                } catch (const std::runtime_error &e) {
                    throw std::runtime_error(std::string(e.what()) + " (decoding " + hex(pc) + ")");
                }
                i.dev_at = dev_at;
                if (i.falls) todo.push_back(uint16_t(pc + i.len));
                for (uint16_t t : i.next) todo.push_back(t);
                for (uint16_t t : i.returns) todo.push_back(t);
                if (i.jp_hl) {
                    int n = 0;
                    const int t2 = io2 ? indexed_table(pc, n) : -1;
                    if (t2 >= 0) {
                        for (int k = 0; k < n; k++) todo.push_back(word(uint32_t(t2 + 2 * k)));
                    } else if (io2) { // another table (the BIOS's diagnostic menus): no code from it
                        std::fprintf(stderr, "dsbrecomp: JP (HL) at %s: no table seen\n", hex(pc).c_str());
                    } else {
                        const int t = dispatch_table(pc);
                        if (t < 0) throw std::runtime_error("JP (HL) without a dispatch table at " + hex(pc));
                        tables.insert(t);
                    }
                }
                code.emplace(pc, std::move(i));
            }
            // A table's entries run up to the next instruction (or the next table).
            for (int t : tables)
                for (int a = t; a + 1 < 0x8000 && !code.count(uint16_t(a)) && !(a != t && tables.count(a)); a += 2) {
                    const uint16_t e = uint16_t(rom[a] | rom[a + 1] << 8);
                    if (e >= 0x8000) break;
                    if (!code.count(e)) todo.push_back(e);
                }
            if (todo.empty()) break;
        }
        bool interrupts = false;
        for (const auto &[pc, i] : code)
            if (i.code.find("z.iff1 = z.iff2 = true") != std::string::npos) interrupts = true;

        std::ofstream o(argv[2]);
        if (io2)
            o << "// Generated by tools/dsbrecomp --io2 from the I/O board's BIOS. ROM-derived: do not commit.\n"
                 "#include \"model1/io2_z80.h\"\n\n#include <bit>\n#include <cstring>\n#include <stdexcept>\n#include <string>\n#include <utility>\n\n"
                 "namespace io2 {\n"
              << kPreambleIo2 << helpers() << kTakeIrqIo2;
        else
            o << "// Generated by tools/dsbrecomp from the DSB's Z80 program. ROM-derived: do not commit.\n"
                 "#include \"model1/dsb_z80.h\"\n\n#include <bit>\n#include <stdexcept>\n#include <string>\n#include <utility>\n\n"
                 "namespace dsb {\n"
              << kPreamble;
        o << "void run(Z80 &z, uint64_t until) {\ndispatch:\n    switch (z.pc) {\n";
        for (const auto &[pc, i] : code) o << "    case " << hex(pc) << ": goto " << lbl(pc) << ";\n";
        o << "    default: throw std::runtime_error(\"" << (io2 ? "I/O board" : "DSB")
          << " Z80: no code at \" + std::to_string(z.pc));\n    }\n";
        for (auto it = code.begin(); it != code.end(); ++it) {
            const uint16_t pc = it->first;
            const Insn &i = it->second;
            if (io2 && i.dev_at > 0)
                o << lbl(pc) << ":\n    if (z.cycles + " << i.dev_at << " >= until) { z.pc = " << hex(pc) << "; return; }\n";
            else
                o << lbl(pc) << ":\n    if (z.cycles >= until) { z.pc = " << hex(pc) << "; return; }\n";
            if (io2) // MAME check_interrupts, before each instruction; the vector's routine by dispatch
                o << "    if (z.iff1 && !z.after_ei && z.io->irq(z.cycles)) { take_irq(z, " << hex(pc)
                  << "); goto dispatch; }\n    z.after_ei = false;\n    z.start = z.cycles;\n";
            else if (interrupts) // MAME check_interrupts, before each instruction
                o << "    if (z.iff1 && !z.after_ei && z.io->irq(z.cycles)) { take_irq(z, " << hex(pc)
                  << "); goto L_0038; }\n    z.after_ei = false;\n";
            if (i.cycles) o << "    z.cycles += " << i.cycles << ";\n";
            if (!i.code.empty()) o << "    " << i.code << "\n";
            if (i.falls) {
                const uint16_t n = uint16_t(pc + i.len);
                const auto nx = std::next(it);
                if (nx == code.end() || nx->first != n) o << "    goto " << lbl(n) << ";\n";
            }
        }
        for (uint16_t pc : outside)
            o << lbl(pc) << ":\n    throw std::runtime_error(\"I/O board Z80: a jump to " << hex(pc) << "\");\n";
        o << "}\n\n} // namespace " << (io2 ? "io2" : "dsb") << "\n";
        if (!o) throw std::runtime_error(std::string("cannot write ") + argv[2]);
        std::printf("dsbrecomp: %zu instructions, %zu dispatch tables%s\n", code.size(), tables.size(),
                    interrupts ? ", interrupts" : "");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "dsbrecomp: %s\n", e.what());
        return 1;
    }
}
