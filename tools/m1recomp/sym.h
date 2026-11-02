// The recompiler's symbolic evaluator for V60 addressing modes: MAME's own
// addressing-mode and operand-decoder code (src/v60/mame/sym_am.inc, the
// same source the interpreter runs) evaluated at recompile time. Everything
// the instruction bytes determine (mode, register numbers, displacements,
// lengths, PC-relative addresses) folds to constants; what depends on the
// machine (register contents, memory) becomes C++ the generated code runs,
// emitted in the order MAME's code does it: each memory access, register
// update and operand store is one statement.
#pragma once

#include "v60/core.h"

#include <cstdint>
#include <string>
#include <vector>

namespace m1r {

class SymAm;

// A 32-bit value: a constant, or the name of something the generated code
// computed (a temporary, or a v60::Core member).
struct Sym {
    bool k = true;
    uint32_t v = 0;
    std::string e;
    Sym() = default;
    Sym(uint32_t c) : k(true), v(c) {}
    Sym(int c) : k(true), v(uint32_t(c)) {}
    Sym(unsigned long c) : k(true), v(uint32_t(c)) {}
    static Sym expr(std::string s) {
        Sym r;
        r.k = false;
        r.e = std::move(s);
        return r;
    }
    std::string str() const;
};

Sym operator+(const Sym &a, const Sym &b);
Sym operator-(const Sym &a, const Sym &b);
Sym operator*(const Sym &a, const Sym &b);
Sym operator/(const Sym &a, const Sym &b);
Sym operator&(const Sym &a, const Sym &b);
Sym operator|(const Sym &a, const Sym &b);
Sym operator^(const Sym &a, const Sym &b);
Sym operator<<(const Sym &a, const Sym &b);
Sym operator>>(const Sym &a, const Sym &b);
Sym operator~(const Sym &a);

// A v60::Core member the generated code stores to (m_amout, m_op1, ...).
class Var {
public:
    Var(SymAm &s, const char *name, unsigned bits) : s_(s), name_(name), bits_(bits) {}
    Var &operator=(const Sym &v);
    Var &operator=(const Var &v) { return *this = v.get(); }
    Var &operator&=(const Sym &v) { return *this = get() & v; }
    Var &operator+=(const Sym &v) { return *this = get() + v; }
    operator Sym() const { return get(); }
    Sym get() const;
    const Sym &value() const { return val_; }
    const char *name() const { return name_; }
    void reset(const Sym &v) { val_ = v; }
    bool assigned = false;

private:
    SymAm &s_;
    const char *name_;
    unsigned bits_;
    Sym val_;
};

// c.m_reg[n]: reads snapshot it into a temporary; updates are statements.
class RegRef {
public:
    RegRef(SymAm &s, unsigned n) : s_(s), n_(n) {}
    operator Sym() const;
    RegRef &operator=(const Sym &v);
    RegRef &operator=(const RegRef &r) { return *this = Sym(r); }
    RegRef &operator+=(const Sym &v);
    RegRef &operator-=(const Sym &v);
    uint32_t operator++(int);
    uint32_t operator--(int);
    RegRef &operator++() { return *this += 1; }
    RegRef &operator--() { return *this -= 1; }

private:
    SymAm &s_;
    unsigned n_;
};
Sym operator+(const RegRef &a, const Sym &b);
Sym operator*(const RegRef &a, const Sym &b);
Sym operator&(const RegRef &a, const Sym &b);
Sym sym_cast_uint8_t(const RegRef &r);
Sym sym_cast_uint16_t(const RegRef &r);

class SymRegs {
public:
    explicit SymRegs(SymAm &s) : s_(s) {}
    RegRef operator[](unsigned n) const { return RegRef(s_, n & 0x7f); }

private:
    SymAm &s_;
};

// c.m_program / c.m_io accesses
class SymMem {
public:
    SymMem(SymAm &s, const char *space) : s_(s), space_(space) {}
    Sym read_byte(const Sym &a);
    Sym read_word_unaligned(const Sym &a);
    Sym read_dword_unaligned(const Sym &a);
    Sym read_dword(const Sym &a) { return read_dword_unaligned(a); }
    void write_byte(const Sym &a, const Sym &v);
    void write_word_unaligned(const Sym &a, const Sym &v);
    void write_dword_unaligned(const Sym &a, const Sym &v);

private:
    Sym read(const char *fn, const Sym &a);
    void write(const char *fn, const Sym &a, const Sym &v);
    SymAm &s_;
    const char *space_;
};

// Instruction bytes, from the ROM (or RAM) image being recompiled.
struct Code {
    const uint8_t *image = nullptr;
    uint32_t mask = 0xffffff;
    uint8_t r8(uint32_t a) const { return image[a & mask]; }
    uint16_t r16(uint32_t a) const { return uint16_t(r8(a) | r8(a + 1) << 8); }
    uint32_t r32(uint32_t a) const { return uint32_t(r16(a)) | uint32_t(r16(a + 2)) << 16; }
};

class SymAm {
public:
    typedef uint32_t (SymAm::*am_func)();

    explicit SymAm(const Code &code) : code_(code) {}

    // Run one hook (a decoder or addressing-mode entry) for the instruction
    // at the concrete core's PC, reading the constants it needs from the core
    // (m_modadd, m_moddim, m_modm, m_instflags, m_subop) and writing back
    // what it computed: constants as they are, machine-dependent values as
    // the dummy `dummy`. Returns the C++ it emitted.
    std::string F12DecodeFirstOperand(v60::Core &c, v60::Am k1, uint8_t d1, uint32_t dummy);
    std::string F12WriteSecondOperand(v60::Core &c, uint8_t d2, uint32_t dummy);
    std::string F12DecodeOperands(v60::Core &c, v60::Am k1, uint8_t d1, v60::Am k2, uint8_t d2, uint32_t dummy);
    std::string F2DecodeFirstOperand(v60::Core &c, v60::Am k1, uint8_t d1, uint32_t dummy);
    std::string F2DecodeSecondOperand(v60::Core &c, v60::Am k2, uint8_t d2, uint32_t dummy);
    std::string F2WriteSecondOperand(v60::Core &c, uint8_t d2, uint32_t dummy);
    std::string F7aDecodeOperands(v60::Core &c, v60::Am k1, uint8_t d1, v60::Am k2, uint8_t d2, uint32_t dummy);
    std::string F7bDecodeFirstOperand(v60::Core &c, v60::Am k1, uint8_t d1, uint32_t dummy);
    std::string F7bWriteSecondOperand(v60::Core &c, uint8_t d2, uint32_t dummy);
    std::string F7bDecodeOperands(v60::Core &c, v60::Am k1, uint8_t d1, v60::Am k2, uint8_t d2, uint32_t dummy);
    std::string F7cDecodeOperands(v60::Core &c, v60::Am k1, uint8_t d1, v60::Am k2, uint8_t d2, uint32_t dummy);
    // Direct entries; *len = the operand's length
    std::string Entry(v60::Core &c, v60::Am k, uint32_t dummy, uint32_t *len);

    // used by Var / RegRef / SymMem
    void emit(const std::string &s) { out_ += s; out_ += ' '; }
    std::string tmp() { return "t" + std::to_string(temps_++); }
    uint32_t pc() const { return m_pc; }

private:
    void begin(v60::Core &c);
    std::string end(v60::Core &c, uint32_t dummy);
    am_func fn(v60::Am k);

    const Code &code_;
    std::string out_;
    int temps_ = 0;

public:
    // The transplanted code's state, as v60_device names it. Constants are
    // plain integers; machine-dependent ones are Var (stored to the core).
    uint32_t m_pc = 0;
    SymRegs m_reg{*this};
    SymMem m_program_mem{*this, "c.m_program"}, m_io_mem{*this, "c.m_io"};
    SymMem *m_program = &m_program_mem;
    SymMem *m_io = &m_io_mem;
    // A constant field and whether the hook wrote it: every write is stored,
    // even of the value the core had at recompile time (at run time it holds
    // whatever the previous instruction left).
    template <class T> struct Field {
        T v = 0;
        bool written = false;
        Field &operator=(T x) {
            v = x;
            written = true;
            return *this;
        }
        operator T() const { return v; }
        void load(T x) {
            v = x;
            written = false;
        }
    };
    Field<uint32_t> m_modadd;
    Field<uint8_t> m_moddim, m_modm, m_modval, m_modval2, m_instflags, m_subop;
    Field<uint8_t> m_amflag, m_flag1, m_flag2;
    Field<uint32_t> m_amlength1, m_amlength2;
    Var m_amout{*this, "m_amout", 32}, m_bamoffset{*this, "m_bamoffset", 32};
    Var m_op1{*this, "m_op1", 32}, m_op2{*this, "m_op2", 32};
    Var m_lenop1{*this, "m_lenop1", 32}, m_lenop2{*this, "m_lenop2", 32};
    Var m_bamoffset1{*this, "m_bamoffset1", 32}, m_bamoffset2{*this, "m_bamoffset2", 32};
    // set by the op handler before a write hook; read here as the core's member
    Sym m_modwritevalb = Sym::expr("c.m_modwritevalb");
    Sym m_modwritevalh = Sym::expr("c.m_modwritevalh");
    Sym m_modwritevalw = Sym::expr("c.m_modwritevalw");

    uint8_t op_read8(uint32_t a) const { return code_.r8(a); }
    uint16_t op_read16(uint32_t a) const { return code_.r16(a); }
    uint32_t op_read32(uint32_t a) const { return code_.r32(a); }

#include "v60/mame/am_decls.inc"
};

} // namespace m1r
