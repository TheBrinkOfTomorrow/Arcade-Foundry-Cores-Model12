#!/usr/bin/env python3
"""Transplant MAME's SHARC (ADSP-21062) semantics (BSD-3-Clause) into
src/runtime/sharc/mame.

    scripts/transplant_sharc.py [MAME_DIR]

Reads src/devices/cpu/sharc from MAME_DIR (default extern/mame, at the commit
pinned in scripts/fetch_mame.sh) and writes, under src/runtime/sharc/mame:

  decls.inc    sharc.h's flag constants, register types and the declarations
               of every handler and helper, for the body of rt::sharc::Sharc
  state.inc    sharcinternal.ipp: the register file (sharc_internal_state),
               the memory accessors and the opcode field helpers
  ops.inc      sharcops.hxx: every opcode handler, the condition codes, the
               stacks and the system registers
  compute.inc  compute.hxx: the ALU, multiplier and shifter operations
  device.inc   from sharc.cpp: CHANGE_PC, internal memory (pm_r/pm_w, short
               words), the IOP registers and check_interrupts
  boot.inc     from sharc.cpp: external_iop_write and external_dma_write (the
               host's port and boot)
  tables.inc   the reciprocal and square-root seed tables (compute.hxx)
  optable.inc  sharcops_table.cpp's opcode table, as names for m2sharcrecomp

The handlers are MAME's, unchanged: the device framework is replaced by
src/runtime/sharc/sharc.h (ours). Every definition but boot.inc's (compiled
once, in src/runtime/sharc/sharc.cpp) is made `inline`, so the
recompiled program (tools/m2sharcrecomp) compiles each instruction's handler
with its opcode as a constant. THIRD_PARTY.md records the commit.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAME = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'extern', 'mame')
SRC = os.path.join(MAME, 'src', 'devices', 'cpu', 'sharc')
OUT = os.path.join(ROOT, 'src', 'runtime', 'sharc', 'mame')

HEADER = ('// license:BSD-3-Clause\n// copyright-holders:Ville Linde\n'
          '// Transplanted from MAME src/devices/cpu/sharc by scripts/transplant_sharc.py:\n'
          '// regenerate, do not edit (THIRD_PARTY.md).\n')


def read(name):
    with open(os.path.join(SRC, name)) as f:
        return f.read()


def strip_license(s):
    return re.sub(r'^// license:.*\n// copyright-holders:.*\n', '', s)


def between(s, start, end, include_end=True):
    i = s.index(start)
    j = s.index(end, i)
    return s[i:j + len(end)] if include_end else s[i:j]


def block_from(s, start):
    """From `start` to the end of the brace block that follows it (and a ';')."""
    i = s.index(start)
    j = s.index('{', i)
    depth = 0
    while True:
        c = s[j]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                break
        j += 1
    j += 1
    if s[j] == ';':
        j += 1
    return s[i:j]


def device_glue(s):
    """MAME's device framework, replaced by rt::sharc::Sharc's."""
    s = s.replace('throw emu_fatalerror(', 'throw SharcFatal(')
    s = re.sub(r'(?<![\w:])fatalerror\(', 'throw SharcFatal(', s)
    s = s.replace('tag()', '"sharc"')
    s = s.replace('m_dm_short_view.select(0);', 'm_dm_short_se = true;')
    s = s.replace('m_dm_short_view.disable();', 'm_dm_short_se = false;')
    s = re.sub(r'm_flag_out_cb\[(\w+)\]\(', r'flag_out(\1, ', s)
    s = s.replace('machine().side_effects_disabled()', 'false')
    s = s.replace('emu_timer *', 'void *')
    s = s.replace('ATTR_COLD', '')
    s = re.sub(r'\n\t*LOG\(.*\);', '', s)
    return s


DEF = re.compile(r'^((?:adsp21062_device::SHARC_REG|std::pair<adsp21062_device::SHARC_REG, adsp21062_device::SHARC_REG>|'
                 r'void|uint32_t|uint64_t|int) adsp21062_device::)', re.M)


def make_inline(s):
    s = DEF.sub(r'inline \1', s)
    s = re.sub(r'^(template <unsigned N>\n)(?!inline)', r'\1inline ', s, flags=re.M)
    return s


def write(name, text):
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, name), 'w') as f:
        f.write(HEADER + text.rstrip('\n') + '\n')


def main():
    h = read('sharc.h')
    decls = []
    decls.append(between(h, 'template <unsigned N> uint64_t pm_r', 'void iop_w(offs_t offset, uint32_t data);'))
    decls.append(block_from(h, 'enum ASTAT_SHIFT'))
    decls.append(block_from(h, 'enum MODE1_DELAY_MODE'))
    decls.append(between(h, '// ASTAT flags', 'MODE2_CAFRZ'))
    decls[-1] = decls[-1] + h[h.index('MODE2_CAFRZ') + len('MODE2_CAFRZ'):].split('\n', 1)[0]
    decls.append(between(h, 'using opcode_func', 'struct alignas(16) sharc_internal_state;'))
    decls.append(between(h, 'static const uint32_t recips_mantissa_lookup', '\n'))
    decls.append(between(h, 'static const uint32_t rsqrts_mantissa_lookup', '\n'))
    fns = between(h, 'inline void CHANGE_PC(uint32_t newpc);', 'inline void compute_multi_reg_to_mr(int ai, int rk);')
    fns = '\n'.join(l for l in fns.split('\n') if 'TIMER_CALLBACK_MEMBER' not in l)
    decls.append(fns)
    # only the opcode table's type is kept; the table itself is optable.inc
    text = '\n\n'.join(decls)
    text = text.replace('\tclass frontend;\n', '').replace('\tclass opcode_desc;\n', '')
    write('decls.inc', device_glue(text))

    ipp = read('sharcinternal.ipp')
    ipp = strip_license(ipp).replace('#include "sharc.h"\n', '')
    ipp = re.sub(r'^#(ifndef|define|endif).*SHARCINTERNAL_IPP.*\n', '', ipp, flags=re.M)
    write('state.inc', device_glue(ipp))

    ops = strip_license(read('sharcops.hxx'))
    ops = ops.replace('#include "compute.hxx"', '#include "runtime/sharc/mame/compute.inc"')
    write('ops.inc', make_inline(device_glue(ops)))

    comp = strip_license(read('compute.hxx'))
    tables = []
    for name in ('recips_mantissa_lookup', 'rsqrts_mantissa_lookup'):
        t = block_from(comp, 'const uint32_t adsp21062_device::' + name)
        comp = comp.replace(t, '')
        tables.append(t)
    # float to int32 as MAME's x86-64 build converts (cvttss2si: out of range
    # and NaN give 0x80000000), on any host.
    comp, n = re.subn(r'alu_i = int32_t\((floorf\(r_alu\.f\)|nearbyintf\(r_alu\.f\)|r_alu\.f)\);', r'alu_i = x86_f2i(\1);', comp)
    assert n == 6, n
    write('compute.inc', make_inline(device_glue(comp)))
    write('tables.inc', '\n\n'.join(tables))

    cpp = read('sharc.cpp')
    dev = []
    for start in ('void adsp21062_device::CHANGE_PC(', 'void adsp21062_device::CHANGE_PC_DELAYED(',
                  'template <unsigned N>\nuint64_t adsp21062_device::pm_r(',
                  'template <unsigned N>\nvoid adsp21062_device::pm_w(',
                  'template <unsigned N>\nuint32_t adsp21062_device::dm_short_r(',
                  'template <unsigned N>\nuint32_t adsp21062_device::dm_short_se_r(',
                  'template <unsigned N>\nvoid adsp21062_device::dm_short_w(',
                  'uint32_t adsp21062_device::iop_r(', 'void adsp21062_device::iop_w(',
                  'void adsp21062_device::check_interrupts('):
        dev.append(block_from(cpp, start))
    write('device.inc', make_inline(device_glue('\n\n'.join(dev))))
    write('boot.inc', device_glue(block_from(cpp, 'void adsp21062_device::external_iop_write(') + '\n\n' +
                                  block_from(cpp, 'void adsp21062_device::external_dma_write(')))

    tab = read('sharcops_table.cpp')
    rows = re.findall(r'\{\s*(0x[0-9a-f]+),\s*(0x[0-9a-f]+),\s*&adsp21062_device::(\w+)\s*\}', tab)
    write('optable.inc', '\n'.join('{%s, %s, "%s"},' % r for r in rows))
    print('wrote %s (%d opcode table rows)' % (os.path.relpath(OUT, ROOT), len(rows)))


if __name__ == '__main__':
    main()
