#!/usr/bin/env python3
"""Transplant MAME's MB86235 "TGPx4" interpreter (BSD-3-Clause) into
src/runtime/tgpx4/mame.

    scripts/transplant_tgpx4.py [MAME_DIR]

Reads src/devices/cpu/mb86235 from MAME_DIR (default extern/mame, at the
commit pinned in scripts/fetch_mame.sh; the files as committed there, not
the working tree's logging patch) and writes, under src/runtime/tgpx4/mame:

  decls.inc  mb86235.h's register file (mb86235_internal_state) and the
             declarations of the interpreter's functions, for the body of
             rt::tgpx4::Tgpx4
  ops.inc    mb86235ops.cpp: the status flags, the step helpers of
             execute_run, the addressing modes, the buses, the ALU and
             multiplier, every transfer type and the control operations

The functions are MAME's, unchanged: the device framework is replaced by
src/runtime/tgpx4/tgpx4.h (ours). Every definition is made `inline`, so the
recompiled program (tools/m2tgpx4recomp) compiles each instruction with its
opcode as a constant. THIRD_PARTY.md records the commit.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAME = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'extern', 'mame')
SRC = 'src/devices/cpu/mb86235/'
OUT = os.path.join(ROOT, 'src', 'runtime', 'tgpx4', 'mame')

HEADER = ('// license:BSD-3-Clause\n// copyright-holders:Angelo Salese, ElSemi, Matthew Daniels\n'
          '// Transplanted from MAME src/devices/cpu/mb86235 by scripts/transplant_tgpx4.py:\n'
          '// regenerate, do not edit (THIRD_PARTY.md).\n')


def read(name):
    return subprocess.run(['git', '-C', MAME, 'show', 'HEAD:' + SRC + name], check=True,
                          capture_output=True, text=True).stdout


def between(s, start, end, include_end=True):
    i = s.index(start)
    j = s.index(end, i)
    return s[i:j + len(end)] if include_end else s[i:j]


def device_glue(s):
    """MAME's device framework, replaced by rt::tgpx4::Tgpx4's."""
    s = re.sub(r'(?<![\w:])fatalerror\(', 'throw Tgpx4Fatal(', s)
    return s


def write(name, text):
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, name), 'w') as f:
        f.write(HEADER + text.rstrip('\n') + '\n')


def main():
    h = read('mb86235.h')
    decls = [between(h, 'struct mb86235_flags', 'mb86235_internal_state  *m_core;'),
             between(h, '// interpreter', 'void handle_single_step_execution();'),
             'bool check_previous_op_stall();']
    write('decls.inc', device_glue('\n\n'.join(decls)))

    ops = read('mb86235ops.cpp')
    ops = ops[ops.index('/*********************\n *\n * Misc helpers'):]
    ops = re.sub(r'^(inline )?(void|bool|uint32_t) mb86235_device::', r'inline \2 mb86235_device::', ops, flags=re.M)
    # float to int as MAME's x86-64 build converts (cvttss2si), on any host
    ops, n = re.subn(r'int v = int\(f\);', 'int v = x86_f2i(f);', ops)
    assert n == 1, n
    ops, n = re.subn(r'uint32_t res = uint32_t\(f\);', 'uint32_t res = x86_f2u(f);', ops)
    assert n == 1, n
    write('ops.inc', device_glue(ops))
    print('wrote %s' % os.path.relpath(OUT, ROOT))


if __name__ == '__main__':
    main()
