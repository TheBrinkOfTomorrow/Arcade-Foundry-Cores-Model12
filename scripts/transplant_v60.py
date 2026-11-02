#!/usr/bin/env python3
"""Transplant MAME's V60 executor (BSD-3-Clause) into src/v60/mame.

    scripts/transplant_v60.py [MAME_DIR]

Reads src/devices/cpu/v60 from MAME_DIR (default extern/mame, at the commit
pinned in scripts/fetch_mame.sh; run it on a clean checkout, before
patches/mame/0006 is applied) and writes, under src/v60/mame:

  macros.inc / undef.inc   v60.cpp's flag and register macros
  ops.inc                  every op handler as `template <class H> uint32_t
                           opX()`, inlined in v60::Core. The operand decoders
                           and addressing-mode entry points they call become
                           calls to the hook class H (H::F12DecodeOperands,
                           H::ReadAM, ...): the interpreter's hooks run MAME's
                           decoders, the recompiler's are generated per
                           instruction with every addressing mode resolved.
  tables.inc               the opcode tables as Tables<H>
  am_decls.inc / am.inc    MAME's addressing modes and operand decoders as
                           v60::Core members (the interpreter, src/v60/interp)
  sym_am.inc               the same addressing modes and decoders for the
                           recompiler's symbolic evaluator (tools/m1recomp)

The op handlers themselves are unchanged: the semantics are MAME's, which is
the oracle. THIRD_PARTY.md records the commit.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAME = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'extern', 'mame')
SRC = os.path.join(MAME, 'src', 'devices', 'cpu', 'v60')
OUT = os.path.join(ROOT, 'src', 'v60', 'mame')

HEADER = ('// license:BSD-3-Clause\n// copyright-holders:Farfetch\'d, R. Belmont\n'
          '// Transplanted from MAME src/devices/cpu/v60 by scripts/transplant_v60.py:\n'
          '// regenerate, do not edit (THIRD_PARTY.md).\n')

DECODERS = ['F12DecodeFirstOperand', 'F12WriteSecondOperand', 'F12DecodeOperands',
            'F2DecodeFirstOperand', 'F2DecodeSecondOperand', 'F2WriteSecondOperand',
            'F7aDecodeOperands', 'F7bDecodeFirstOperand', 'F7bWriteSecondOperand',
            'F7bDecodeOperands', 'F7cDecodeOperands']
AM_ENTRIES = ['BitReadAMAddress', 'ReadAMAddress', 'BitReadAM', 'ReadAM', 'WriteAM']
AM_KIND = {'ReadAM': 'Am::Read', 'BitReadAM': 'Am::BitRead', 'ReadAMAddress': 'Am::Addr',
           'BitReadAMAddress': 'Am::BitAddr', 'WriteAM': 'Am::Write'}


def read(name):
    with open(os.path.join(SRC, name)) as f:
        return f.read()


def strip_license(s):
    return re.sub(r'^// license:.*\n// copyright-holders:.*\n', '', s)


def split_functions(s):
    """Yields ('text', str) and ('func', ret, name, args, body) and
    ('table', type, name, size, entries) pieces of a MAME source file."""
    pos = 0
    pat = re.compile(r'^(uint32_t|void|const v60_device::(?:am_func|op6_func)) v60_device::(\w+)(\[[^\]]*\](?:\[[^\]]*\])?)?(\([^)]*\))?[^\n{=]*(=)?[^\n{]*\n?\{', re.M)
    while True:
        m = pat.search(s, pos)
        if not m:
            yield ('text', s[pos:])
            return
        yield ('text', s[pos:m.start()])
        # find the matching close brace
        depth, i = 0, m.end() - 1
        while True:
            c = s[i]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    break
            i += 1
        body = s[m.end():i]
        end = i + 1
        if m.group(5):  # table
            if s[end:end + 1] == ';':
                end += 1
            yield ('table', m.group(1), m.group(2), m.group(3), body)
        else:
            yield ('func', m.group(1), m.group(2), m.group(4) or '()', body)
        pos = end


def hookify(body):
    """Op handler body: decoder and addressing-mode calls go to H."""
    for d in DECODERS:
        body = re.sub(r'\b' + d + r'\(\s*\)', 'H::' + d + '(*this)', body)
        body = re.sub(r'\b' + d + r'\(', 'H::' + d + '(*this, ', body)
    for a in AM_ENTRIES:
        body = re.sub(r'(?<![:\w])' + a + r'\(\)', 'H::' + a + '(*this)', body)
    for a, k in sorted(AM_KIND.items(), key=lambda x: -len(x[0])):
        body = body.replace('&v60_device::' + a, k)
    body = re.sub(r'\(this->\*(s_\w+Table)\[', r'(this->*Tables<H>::\1[', body)
    body = re.sub(r'(?<![\w:>])(op[A-Z0-9]\w*)\(', r'\1<H>(', body)
    return body


def common(body):
    body = body.replace('fatalerror(', 'v60_fatal(')
    body = body.replace('logerror(', 'v60_log(').replace('osd_printf_debug(', 'v60_log(')
    return body


def main():
    os.makedirs(OUT, exist_ok=True)
    v60cpp = read('v60.cpp')
    a = v60cpp.index('// memory accessors')
    b = v60cpp.index('void v60_device::v60SaveStack()')
    macros = v60cpp[a:b]
    g = v60cpp.index('#define GETINTVECT')
    macros += v60cpp[g:v60cpp.index('\n\n\n', g)] + '\n'
    macros = macros.replace('#define OpRead8(a)   m_pr8(a)', '#define OpRead8(a)   m_program->op_read8(a)')
    macros = macros.replace('#define OpRead16(a)  m_pr16(a)', '#define OpRead16(a)  m_program->op_read16(a)')
    macros = macros.replace('#define OpRead32(a)  m_pr32(a)', '#define OpRead32(a)  m_program->op_read32(a)')
    defined = []

    ops, tables, am, decls, symam = [], [], [], [], []
    files = ['op12.hxx', 'op2.hxx', 'op3.hxx', 'op4.hxx', 'op5.hxx', 'op6.hxx', 'op7a.hxx']
    for fname in ['am1.hxx', 'am2.hxx', 'am3.hxx', 'am.hxx'] + files + ['optable.hxx']:
        src = strip_license(read(fname))
        is_am = fname.startswith('am')
        for piece in split_functions(src):
            if piece[0] == 'text':
                t = re.sub(r'^#include .*\n', '', piece[1], flags=re.M)
                for m in re.finditer(r'^#define\s+(\w+)', t, re.M):
                    defined.append(m.group(1))
                (am if is_am else ops).append(t)
                if is_am:
                    symam.append(t)
            elif piece[0] == 'table':
                _, ttype, name, size, entries = piece
                entries = common(entries)
                if name.startswith('s_AMTable') or name.startswith('s_BAMTable'):
                    decls.append('\tstatic const am_func %s%s;\n' % (name, size))
                    am.append('const Core::am_func Core::%s%s =\n{%s};\n' % (name, size, entries.replace('v60_device::', 'Core::')))
                    symam.append('const SymAm::am_func SymAm::%s%s =\n{%s};\n' % (name, size, entries.replace('v60_device::', 'SymAm::')))
                else:
                    ft = 'Core::op6_func' if 'op6_func' in ttype else 'Core::am_func'
                    ents = re.sub(r'&v60_device::(\w+)', r'&Core::template \1<H>', entries)
                    tables.append('\tstatic constexpr %s %s%s =\n\t{%s};\n' % (ft, name, size, ents))
            else:
                _, ret, name, args, body = piece
                body = common(body)
                if is_am or name in DECODERS:
                    decls.append('\t%s %s%s;\n' % (ret, name, args.replace('am_func', 'am_func')))
                    am.append('%s Core::%s%s\n{%s}\n' % (ret, name, args, body.replace('v60_device::', 'Core::')))
                    symam.append('%s SymAm::%s%s\n{%s}\n' % (ret, name, args, body.replace('v60_device::', 'SymAm::')))
                else:
                    ops.append('template <class H> %s %s%s\n{\n\tH::enter(*this, "%s");%s}\n'
                               % (ret, name, args, name, hookify(body)))
    ops.append('template <class H> uint32_t opUNHANDLED()\n{\n\tH::enter(*this, "opUNHANDLED");\n'
               '\tv60_fatal("Unhandled OpCode found : %02x at %08x\\n", OpRead16(PC), PC);\n}\n')
    for m in re.finditer(r'^#define\s+(\w+)', macros, re.M):
        defined.append(m.group(1))

    def write(name, text):
        with open(os.path.join(OUT, name), 'w') as f:
            f.write(HEADER + text)

    write('macros.inc', '#undef _S\n' + macros)
    write('undef.inc', ''.join('#undef %s\n' % d for d in sorted(set(defined))))
    write('ops.inc', ''.join(ops))
    write('tables.inc', 'template <class H> struct Tables\n{\n' + ''.join(tables) + '};\n')
    write('am_decls.inc', ''.join(decls))
    write('am.inc', ''.join(am))
    sym = ''.join(symam)
    sym = re.sub(r'\((uint8_t|uint16_t)\)(m_reg\[[^\]]*\])', r'sym_cast_\1(\2)', sym)
    write('sym_am.inc', sym)


main()
