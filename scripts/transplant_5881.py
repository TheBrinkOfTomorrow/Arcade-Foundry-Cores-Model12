#!/usr/bin/env python3
"""Transplant MAME's Sega 315-5881 decryption (BSD-3-Clause) into
src/runtime/crypt5881/mame.

    scripts/transplant_5881.py [MAME_DIR]

Reads src/mame/sega/315-5881_crypt.cpp and .h from MAME_DIR (default
extern/mame, at the commit pinned in scripts/fetch_mame.sh) and writes,
under src/runtime/crypt5881/mame:

  decls.inc  the class's state, tables and helpers (315-5881_crypt.h's
             private section), for the body of rt::Crypt5881
  crypt.inc  315-5881_crypt.cpp from the chip's I/O interface on: the
             register handlers, the stream cipher, its tables and the
             decompression

The functions are MAME's, unchanged: the device framework (device type,
address maps, constructor, device_start/device_reset, logging) is replaced
by src/runtime/crypt5881/crypt5881.h (ours). THIRD_PARTY.md records the
commit.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAME = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'extern', 'mame')
SRC = 'src/mame/sega/'
OUT = os.path.join(ROOT, 'src', 'runtime', 'crypt5881', 'mame')

HEADER = ('// license:BSD-3-Clause\n// copyright-holders:Andreas Naive, Olivier Galibert, David Haywood\n'
          '// Transplanted from MAME src/mame/sega/315-5881_crypt.cpp by scripts/transplant_5881.py:\n'
          '// regenerate, do not edit (THIRD_PARTY.md).\n')


def read(name):
    return subprocess.run(['git', '-C', MAME, 'show', 'HEAD:' + SRC + name], check=True,
                          capture_output=True, text=True).stdout


def main():
    h = read('315-5881_crypt.h')
    c = read('315-5881_crypt.cpp')
    decls = h[h.index('private:\n') + len('private:\n'):h.index('};\n\n#endif')]
    body = c[c.index('uint16_t sega_315_5881_crypt_device::ready_r()'):]
    body = body.replace('sega_315_5881_crypt_device::', 'Crypt5881::')
    body = re.sub(r'\n\tlogerror\([^\n]*\);', '', body)  # logging: no device framework
    body = re.sub(r'\n\tif \(data != 0\)\n\t\tprintf\([^\n]*\);', '', body)
    body = body.replace('offs_t offset, uint16_t data, uint16_t mem_mask', 'uint32_t offset, uint16_t data, uint16_t mem_mask')
    os.makedirs(OUT, exist_ok=True)
    for name, text in (('decls.inc', decls), ('crypt.inc', body)):
        with open(os.path.join(OUT, name), 'w') as f:
            f.write(HEADER + '\n' + text)
    print('transplant_5881: wrote', OUT)


if __name__ == '__main__':
    main()
