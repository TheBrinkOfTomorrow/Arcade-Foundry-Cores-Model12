#include "sym.h"

#include <cstdio>

namespace m1r {

namespace {
std::string hexs(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%xu", v);
    return b;
}
Sym bin(const Sym &a, const Sym &b, const char *op, uint32_t r) {
    if (a.k && b.k) return Sym(r);
    return Sym::expr("uint32_t(" + a.str() + " " + op + " " + b.str() + ")");
}
} // namespace

std::string Sym::str() const { return k ? hexs(v) : e; }

Sym operator+(const Sym &a, const Sym &b) {
    if (b.k && b.v == 0) return a;
    if (a.k && a.v == 0) return b;
    return bin(a, b, "+", a.v + b.v);
}
Sym operator-(const Sym &a, const Sym &b) { return bin(a, b, "-", a.v - b.v); }
Sym operator*(const Sym &a, const Sym &b) {
    if (b.k && b.v == 1) return a;
    return bin(a, b, "*", a.v * b.v);
}
Sym operator/(const Sym &a, const Sym &b) {
    if (b.k && b.v == 0) throw v60::Fatal("division by zero in an addressing mode");
    return bin(a, b, "/", b.v ? a.v / b.v : 0);
}
Sym operator&(const Sym &a, const Sym &b) { return bin(a, b, "&", a.v & b.v); }
Sym operator|(const Sym &a, const Sym &b) { return bin(a, b, "|", a.v | b.v); }
Sym operator^(const Sym &a, const Sym &b) { return bin(a, b, "^", a.v ^ b.v); }
Sym operator<<(const Sym &a, const Sym &b) { return bin(a, b, "<<", b.v < 32 ? a.v << b.v : 0); }
Sym operator>>(const Sym &a, const Sym &b) { return bin(a, b, ">>", b.v < 32 ? a.v >> b.v : 0); }
Sym operator~(const Sym &a) { return a.k ? Sym(~a.v) : Sym::expr("uint32_t(~" + a.str() + ")"); }

Var &Var::operator=(const Sym &v) {
    Sym x = v;
    if (bits_ < 32) x = x & Sym((1u << bits_) - 1);
    s_.emit(std::string("c.") + name_ + " = " + x.str() + ";");
    val_ = x.k ? x : Sym::expr(std::string("c.") + name_);
    assigned = true;
    return *this;
}
Sym Var::get() const { return val_; }

RegRef::operator Sym() const {
    if (n_ == 32) return Sym(s_.pc()); // PC: the instruction's address
    const std::string t = s_.tmp();
    s_.emit("const uint32_t " + t + " = c.m_reg[" + std::to_string(n_) + "];");
    return Sym::expr(t);
}
RegRef &RegRef::operator=(const Sym &v) {
    if (n_ == 32) throw v60::Fatal("an addressing mode writes PC");
    s_.emit("c.m_reg[" + std::to_string(n_) + "] = " + v.str() + ";");
    return *this;
}
RegRef &RegRef::operator+=(const Sym &v) {
    s_.emit("c.m_reg[" + std::to_string(n_) + "] += " + v.str() + ";");
    return *this;
}
RegRef &RegRef::operator-=(const Sym &v) {
    s_.emit("c.m_reg[" + std::to_string(n_) + "] -= " + v.str() + ";");
    return *this;
}
uint32_t RegRef::operator++(int) {
    *this += 1;
    return 0;
}
uint32_t RegRef::operator--(int) {
    *this -= 1;
    return 0;
}
Sym operator+(const RegRef &a, const Sym &b) { return Sym(a) + b; }
Sym operator*(const RegRef &a, const Sym &b) { return Sym(a) * b; }
Sym operator&(const RegRef &a, const Sym &b) { return Sym(a) & b; }
Sym sym_cast_uint8_t(const RegRef &r) { return Sym(r) & Sym(0xffu); }
Sym sym_cast_uint16_t(const RegRef &r) { return Sym(r) & Sym(0xffffu); }

Sym SymMem::read(const char *fn, const Sym &a) {
    const std::string t = s_.tmp();
    s_.emit("const uint32_t " + t + " = " + space_ + "->" + fn + "(" + a.str() + ");");
    return Sym::expr(t);
}
void SymMem::write(const char *fn, const Sym &a, const Sym &v) {
    s_.emit(std::string(space_) + "->" + fn + "(" + a.str() + ", " + v.str() + ");");
}
Sym SymMem::read_byte(const Sym &a) { return read("read_byte", a); }
Sym SymMem::read_word_unaligned(const Sym &a) { return read("read_word_unaligned", a); }
Sym SymMem::read_dword_unaligned(const Sym &a) { return read("read_dword_unaligned", a); }
void SymMem::write_byte(const Sym &a, const Sym &v) { write("write_byte", a, v); }
void SymMem::write_word_unaligned(const Sym &a, const Sym &v) { write("write_word_unaligned", a, v); }
void SymMem::write_dword_unaligned(const Sym &a, const Sym &v) { write("write_dword_unaligned", a, v); }

// ---------------------------------------------------------------------------
// MAME's addressing modes and operand decoders, evaluated symbolically.

using v60::v60_fatal;
using v60::v60_log;

#define OpRead8(a) op_read8(a)
#define OpRead16(a) op_read16(a)
#define OpRead32(a) op_read32(a)
#define PC m_pc
#define SETREG8(a, b) (a) = ((a) & ~0xffu) | ((b) & 0xffu)
#define SETREG16(a, b) (a) = ((a) & ~0xffffu) | ((b) & 0xffffu)
#include "v60/mame/sym_am.inc"
#undef OpRead8
#undef OpRead16
#undef OpRead32
#undef PC
#undef SETREG8
#undef SETREG16

SymAm::am_func SymAm::fn(v60::Am k) {
    switch (k) {
    case v60::Am::Read: return &SymAm::ReadAM;
    case v60::Am::BitRead: return &SymAm::BitReadAM;
    case v60::Am::Addr: return &SymAm::ReadAMAddress;
    case v60::Am::BitAddr: return &SymAm::BitReadAMAddress;
    case v60::Am::Write: return &SymAm::WriteAM;
    }
    return nullptr;
}

namespace {
Var *const *vars_of(SymAm &s, Var *(&v)[8]) {
    v[0] = &s.m_amout;
    v[1] = &s.m_bamoffset;
    v[2] = &s.m_op1;
    v[3] = &s.m_op2;
    v[4] = &s.m_lenop1;
    v[5] = &s.m_lenop2;
    v[6] = &s.m_bamoffset1;
    v[7] = &s.m_bamoffset2;
    return v;
}
uint32_t &core_var(v60::Core &c, int i) {
    switch (i) {
    case 0: return c.m_amout;
    case 1: return c.m_bamoffset;
    case 2: return c.m_op1;
    case 3: return c.m_op2;
    case 4: return c.m_lenop1;
    case 5: return c.m_lenop2;
    case 6: return c.m_bamoffset1;
    default: return c.m_bamoffset2;
    }
}
} // namespace

void SymAm::begin(v60::Core &c) {
    out_.clear();
    m_pc = c.m_reg[32];
    m_modadd.load(c.m_modadd);
    m_moddim.load(c.m_moddim);
    m_modm.load(c.m_modm);
    m_modval.load(c.m_modval);
    m_modval2.load(c.m_modval2);
    m_instflags.load(c.m_instflags);
    m_subop.load(c.m_subop);
    m_amflag.load(c.m_amflag);
    m_flag1.load(c.m_flag1);
    m_flag2.load(c.m_flag2);
    m_amlength1.load(c.m_amlength1);
    m_amlength2.load(c.m_amlength2);
    Var *v[8];
    vars_of(*this, v);
    for (Var *x : v) {
        x->reset(Sym::expr(std::string("c.") + x->name()));
        x->assigned = false;
    }
}

std::string SymAm::end(v60::Core &c, uint32_t dummy) {
    // constants the hook wrote: stored for the handler, copied to the core
    auto u8 = [&](Field<uint8_t> SymAm::*m, uint8_t v60::Core::*cm, const char *name) {
        if ((this->*m).written) {
            emit(std::string("c.") + name + " = " + std::to_string(unsigned((this->*m).v)) + ";");
            c.*cm = (this->*m).v;
        }
    };
    auto u32 = [&](Field<uint32_t> SymAm::*m, uint32_t v60::Core::*cm, const char *name) {
        if ((this->*m).written) {
            emit(std::string("c.") + name + " = " + std::to_string((this->*m).v) + "u;");
            c.*cm = (this->*m).v;
        }
    };
    u32(&SymAm::m_modadd, &v60::Core::m_modadd, "m_modadd");
    u8(&SymAm::m_moddim, &v60::Core::m_moddim, "m_moddim");
    u8(&SymAm::m_modm, &v60::Core::m_modm, "m_modm");
    u8(&SymAm::m_modval, &v60::Core::m_modval, "m_modval");
    u8(&SymAm::m_modval2, &v60::Core::m_modval2, "m_modval2");
    u8(&SymAm::m_instflags, &v60::Core::m_instflags, "m_instflags");
    u8(&SymAm::m_subop, &v60::Core::m_subop, "m_subop");
    u8(&SymAm::m_amflag, &v60::Core::m_amflag, "m_amflag");
    u8(&SymAm::m_flag1, &v60::Core::m_flag1, "m_flag1");
    u8(&SymAm::m_flag2, &v60::Core::m_flag2, "m_flag2");
    u32(&SymAm::m_amlength1, &v60::Core::m_amlength1, "m_amlength1");
    u32(&SymAm::m_amlength2, &v60::Core::m_amlength2, "m_amlength2");
    Var *v[8];
    vars_of(*this, v);
    for (int i = 0; i < 8; i++)
        if (v[i]->assigned) core_var(c, i) = v[i]->value().k ? v[i]->value().v : dummy + uint32_t(i);
    return out_;
}

#define HOOK(name, params, call)                                                                                     \
    std::string SymAm::name params {                                                                                 \
        begin(c);                                                                                                    \
        call;                                                                                                        \
        return end(c, dummy);                                                                                        \
    }
HOOK(F12DecodeFirstOperand, (v60::Core & c, v60::Am k1, uint8_t d1, uint32_t dummy), F12DecodeFirstOperand(fn(k1), d1))
HOOK(F12WriteSecondOperand, (v60::Core & c, uint8_t d2, uint32_t dummy), F12WriteSecondOperand(d2))
HOOK(F12DecodeOperands, (v60::Core & c, v60::Am k1, uint8_t d1, v60::Am k2, uint8_t d2, uint32_t dummy),
     F12DecodeOperands(fn(k1), d1, fn(k2), d2))
HOOK(F2DecodeFirstOperand, (v60::Core & c, v60::Am k1, uint8_t d1, uint32_t dummy), F2DecodeFirstOperand(fn(k1), d1))
HOOK(F2DecodeSecondOperand, (v60::Core & c, v60::Am k2, uint8_t d2, uint32_t dummy), F2DecodeSecondOperand(fn(k2), d2))
HOOK(F2WriteSecondOperand, (v60::Core & c, uint8_t d2, uint32_t dummy), F2WriteSecondOperand(d2))
HOOK(F7aDecodeOperands, (v60::Core & c, v60::Am k1, uint8_t d1, v60::Am k2, uint8_t d2, uint32_t dummy),
     F7aDecodeOperands(fn(k1), d1, fn(k2), d2))
HOOK(F7bDecodeFirstOperand, (v60::Core & c, v60::Am k1, uint8_t d1, uint32_t dummy), F7bDecodeFirstOperand(fn(k1), d1))
HOOK(F7bWriteSecondOperand, (v60::Core & c, uint8_t d2, uint32_t dummy), F7bWriteSecondOperand(d2))
HOOK(F7bDecodeOperands, (v60::Core & c, v60::Am k1, uint8_t d1, v60::Am k2, uint8_t d2, uint32_t dummy),
     F7bDecodeOperands(fn(k1), d1, fn(k2), d2))
HOOK(F7cDecodeOperands, (v60::Core & c, v60::Am k1, uint8_t d1, v60::Am k2, uint8_t d2, uint32_t dummy),
     F7cDecodeOperands(fn(k1), d1, fn(k2), d2))
#undef HOOK

std::string SymAm::Entry(v60::Core &c, v60::Am k, uint32_t dummy, uint32_t *len) {
    begin(c);
    *len = (this->*fn(k))();
    return end(c, dummy);
}

} // namespace m1r
