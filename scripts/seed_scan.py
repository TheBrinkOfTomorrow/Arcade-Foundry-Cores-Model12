#!/usr/bin/env python3
"""Find i960 entry points the MAME harvest never reached, by static analysis
of the user's ROM set, and list them as seeds.

  seed_scan.py [--set SET] [--build-dir build] [--config Release] [--append]

The game calls code it has only as a pointer: state machines store the next
state's address (lda 0x2266f8,r5; st r5,0xc(r3)), jump tables are indexed
(ld 0x18cc[g0*4]; callx, lda 0x225010[r3*4]; ld; callx), and object records
in the program and data ROMs carry handler addresses. A state the harvest did
not visit stops the game with "no recompiled code at X". Candidates:

  - lda constants in reachable code;
  - entries of tables read with a [reg*4] index in reachable code;
  - every aligned word of the program ROM that points into code;
  - words of the main data ROM that point at a function start (after a ret
    or an unconditional branch, the zero words padding after one, or a word
    that is not an instruction; the data ROM is large, so this is stricter).

A candidate is kept only if it decodes as executable code up to a ret or
branch (i960dis: no undecodable, !noexec or !quirk word first). Repeats to a
fixed point, recompiling into a scratch directory each round. A false
positive only costs unused native code; a miss stops the game.

Needs the ROM set imported (scripts/recompile.py has run). Prints the new
addresses; --append adds them to seeds/<set>.txt. Then run
scripts/recompile.py.
"""

import argparse
import glob
import os
import re
import shutil
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recompile  # noqa: E402  (MODEL2A_SETS, CODE_COPY)
from game_sets import M2_SETS  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RET = 0x0A000000
ENDS = {"ret", "b", "bx"}
INSN = re.compile(r"^L_([0-9a-f]{8}): // [0-9a-f]{8}: (\S+)\s+(0x[0-9a-f]+)?(\[\w+\*4\])?")


def tool(build, config, name):
    exe = name + (".exe" if os.name == "nt" else "")
    for p in (os.path.join(build, exe), os.path.join(build, config, exe)):
        if os.path.exists(p):
            return p
    sys.exit("seed_scan: %s not built (run scripts/setup.py)" % name)


def words(data):
    return {v for (v,) in struct.iter_unpack("<I", data[: len(data) & ~3])}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--set", default="daytona93", choices=M2_SETS)
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--config", default="Release")
    ap.add_argument("--append", action="store_true", help="add the new seeds to seeds/<set>.txt")
    args = ap.parse_args()
    build = os.path.join(ROOT, args.build_dir)
    cache = os.path.join(build, "rom_cache", args.set)
    seeds_path = os.path.join(ROOT, "seeds", args.set + ".txt")
    prog_path = os.path.join(cache, "program.bin")
    if not os.path.exists(prog_path):
        sys.exit("seed_scan: no imported ROM set in %s (run scripts/recompile.py)" % cache)
    recomp, dis = tool(build, args.config, "m2recomp"), tool(build, args.config, "i960dis")

    img = open(prog_path, "rb").read()

    # Program ROM at 0, its upper 128 KiB mirrored at 0x220000 (as m2recomp).
    # Model 2A and 2B sets: no mirror; their code runs from the whole program
    # ROM, and from the RAM copies in recompile.py's CODE_COPY (srallyc runs
    # all but its boot code below 0x1000 from its copy of ROM 0x1000-0x60fff
    # at 0x5a0000, so its ROM above 0x1000 is not code).
    model2a = args.set in recompile.MODEL2A_SETS
    copies = [tuple(int(x, 16) for x in c) for c in recompile.CODE_COPY.get(args.set, [])]
    rom_end = 0x1000 if args.set == "srallyc" else len(img)
    recomp_flags = (["--model2a"] + [x for c in recompile.CODE_COPY.get(args.set, []) for x in ("--code-copy", *c)]
                    if model2a else [])
    if args.set in recompile.SYSTEM_PROCEDURES:
        recomp_flags += ["--system-procedures", str(recompile.SYSTEM_PROCEDURES[args.set])]

    def off(a):
        if model2a:
            if 0x800 <= a < rom_end:
                return a
            for src, dst, size in copies:
                if dst <= a < dst + size:
                    return a - dst + src
            return None
        if 0x800 <= a < len(img):
            return a
        if 0x220000 <= a < 0x240000:
            return a - 0x200000
        return None

    def word(a):
        o = off(a)
        return None if o is None or o + 4 > len(img) else struct.unpack_from("<I", img, o)[0]

    def plausible(a):
        return a % 4 == 0 and off(a) is not None

    def fstart(a):
        # after a ret or an unconditional branch, or after up to three zero
        # words that pad to the function after one (vcop's object handlers: 0x336d0)
        for k in range(1, 5):
            w = word(a - 4 * k)
            if w != 0:
                return w is not None and (w == RET or (w >> 24) == 0x08)
        return False

    after = {}

    def after_data(a):
        # after a word that is not an instruction: a function placed straight
        # after a table (fvipers' test mode: 0x59e30, after a byte table)
        if a not in after:
            out = subprocess.run([dis, "--start", hex(off(a) - 4), "--count", "1", prog_path],
                                 capture_output=True, text=True).stdout.split()
            after[a] = len(out) >= 3 and out[2] == "?"
        return after[a]

    decoded = {}

    def is_code(a):
        if a not in decoded:
            out = subprocess.run([dis, "--start", hex(off(a)), "--count", "400", prog_path],
                                 capture_output=True, text=True).stdout
            decoded[a] = False
            for line in out.splitlines():
                parts = line.split()
                if len(parts) < 3 or "!noexec" in line or "!quirk" in line:
                    break
                mn = next((p for p in parts[2:] if not re.fullmatch(r"[0-9a-f]{8}", p)), "?")
                if mn == "?":
                    break
                if mn in ENDS or (mn == "balx" and inline_string(int(parts[0].rstrip(":"), 16) + 4 * sum(
                        1 for p in parts[1:] if re.fullmatch(r"[0-9a-f]{8}", p)))):
                    decoded[a] = True
                    break
        return decoded[a]

    def inline_string(k):
        """The end of a printable, NUL-terminated string at ROM offset k, or None."""
        e = k
        while e < len(img) and 0x20 <= img[e] < 0x7F:
            e += 1
        return e if e - k >= 2 and e < len(img) and img[e] == 0 else None

    rom_ptrs = {v for v in words(img) if plausible(v)}
    # A balx to a print routine followed by its string in line: the routine
    # returns past the string (vf2's test mode: balx 0x9444,r14 then "DISPLAY
    # TEST 1/2"), so the code after it is reached only by that return.
    for o in range(0, len(img) - 8, 4):
        w = struct.unpack_from("<I", img, o)[0]
        if w >> 24 != 0x85:
            continue
        e = inline_string(o + (8 if (w >> 12) & 1 and (w >> 10) & 0xF in (0x5, 0xC, 0xD, 0xE, 0xF) else 4))
        if e is not None:
            c = (e + 4) & ~3  # the next word after the NUL
            if off(c) == c:
                rom_ptrs.add(c)
    data = open(os.path.join(cache, "main_data.bin"), "rb").read()
    data_ptrs = {v for v in words(data) if plausible(v) and (fstart(v) or after_data(v))}
    del data

    seeds = [int(l, 16) for l in open(seeds_path) if l.strip() and not l.startswith("#")]
    found = []
    scratch = os.path.join(build, "seed_scan")
    gen = os.path.join(scratch, "gen")

    def recompile_with(extra):
        shutil.rmtree(scratch, ignore_errors=True)
        os.makedirs(gen)
        sf = os.path.join(scratch, "seeds.txt")
        with open(sf, "w") as f:
            f.writelines("%08x\n" % a for a in seeds + extra)
        return subprocess.run([recomp, prog_path, gen, "--seeds", sf] + recomp_flags, capture_output=True, text=True)

    last = []  # the previous round's new entry points
    dropped = set()
    for rnd in range(1, 50):
        r = recompile_with(found)
        # A candidate that decodes as code but whose branches run into data
        # (an instruction with no template, such as fault): find it by halving
        # the previous round's batch, drop it, and recompile again.
        while r.returncode and "no native template" in r.stderr and last:
            batch = list(last)
            while len(batch) > 1:
                half = batch[: len(batch) // 2]
                rest = [a for a in found if a not in batch]
                batch = half if recompile_with(rest + half).returncode else batch[len(batch) // 2:]
            print("round %d: dropped %08x (its code runs into data)" % (rnd, batch[0]))
            found.remove(batch[0])
            dropped.add(batch[0])
            last.remove(batch[0])
            r = recompile_with(found)
        if r.returncode:
            sys.exit(r.stderr + "\nseed_scan: recompile failed in round %d" % rnd)

        reached, targets, tables = set(), set(rom_ptrs | data_ptrs), set()
        for path in glob.glob(os.path.join(gen, "chunk_*.cpp")):
            for line in open(path):
                m = INSN.match(line)
                if not m:
                    continue
                reached.add(int(m.group(1), 16))
                if m.group(3) and m.group(2) in ("ld", "lda"):
                    if m.group(4):
                        tables.add(int(m.group(3), 16))
                    elif m.group(2) == "lda":
                        targets.add(int(m.group(3), 16))
        for t in tables:
            for j in range(256):  # entries run until one is not a code address
                v = word(t + 4 * j)
                if v is None or not plausible(v) or (t + 4 * j) in reached or not is_code(v):
                    break
                targets.add(v)

        new = sorted(a for a in targets if a not in reached and a not in dropped and plausible(a) and is_code(a))
        print("round %d: %d instructions reachable, %d new entry points" % (rnd, len(reached), len(new)))
        if not new:
            break
        found += new
        last = new
    shutil.rmtree(scratch, ignore_errors=True)

    found = sorted(set(found))
    for a in found:
        print("%08x" % a)
    print("seed_scan: %d new seeds" % len(found), file=sys.stderr)
    if found and args.append:
        with open(seeds_path, "a") as f:
            f.write("# scripts/seed_scan.py\n")
            f.writelines("%08x\n" % a for a in found)
        print("seed_scan: appended to %s; now run scripts/recompile.py" % seeds_path, file=sys.stderr)


if __name__ == "__main__":
    main()
