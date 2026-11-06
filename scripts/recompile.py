#!/usr/bin/env python3
"""Recompile the game's i960 code, its TGP program and the sound board's 68000
program to native C++ and build them. Same steps on Linux, macOS and Windows.

  recompile.py [--set SET] [--build-dir build] [--config Release] [--jobs N] [--fast_inaccuracy]

Needs the user's ROM set at roms/<set>.zip or roms/<set>.7z (git-ignored):
daytona93 (Daytona USA Deluxe '93, the default) or daytona (Revision A,
1994; give it its own build directory, e.g. --build-dir build-daytona).
srallyc (Sega Rally Championship, Revision C, Model 2A-CRX) recompiles
its i960 code for the lockstep check (m2native) only: its board is not
written yet, so there is no game to run.
indy500 (INDY 500), fvipers (Fighting Vipers), sgt24h (Super GT 24h),
overrevb (Over Rev), von (Cyber Troopers Virtual-On), gunblade
(Gunblade NY), vstriker (Virtua Striker), zerogun (Zero Gunner) and pltkids (Pilot
Kids), on Model 2B-CRX,
recompile their SHARC program
(tools/m2sharcrecomp) in place of a TGP program. stcc (Sega Touring Car
Championship, Model 2C-CRX) and hotdo (The House of the Dead, Model 2C-CRX)
recompile their TGPx4 program (tools/m2tgpx4recomp) instead.
vr (Virtua Racing, Model 1) recompiles the V60 code, the TGP program and
the sound 68000 program (cmake/model1.cmake): --core builds its core
library, otherwise m1run, m1native and the other Model 1 tools.
Everything
derived from it (images, generated C++) goes under the build directory,
which is git-ignored: never commit it.

--fast_inaccuracy turns on every recompiler speed option
for slow targets:
  i960 (tools/m2recomp/main.cpp)
  1. the lockstep bookkeeping (interrupt and event checks, instruction count)
     once per basic block instead of before every instruction: no longer
     exact against MAME (interrupts are taken a few instructions later);
  2. direct chaining: the generated gen::run moves from chunk to chunk itself
     instead of returning to GameLoop on each transfer between chunks.
  TGP (tools/m2tgprecomp/main.cpp)
  3. for calls without an instruction budget (the game's): the instruction
     count once per basic block and no budget test at each branch. Results
     and counts stay those of the default output (the TGP has no interrupts).
The lockstep and trace comparisons need the default output. Off by default;
run again without it to go back.
"""

import argparse
import os
import subprocess
import sys

from game_sets import M2_SETS, MODEL1_SETS, MODEL2A_ONLY_SETS, MODEL2B_SETS, MODEL2C_SETS, OTHER_NAMES

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# Exit status for "the ROM set was rejected", so setup.py can tell it from a
# failure to recompile or build (which exit 1, with the tool's own errors).
ROM_REJECTED_EXIT = 3

# Sets on the Model 2A-CRX board (CMake's M2_BOARD model2a) and on Model
# 2B-CRX (model2b: the same board with a SHARC for the TGP).
# 2B-CRX (model2b: the same board with a SHARC for the TGP) and 2C-CRX (model2c:
# a TGPx4 for the SHARC). The set lists come from games.toml (scripts/game_sets.py).
DSB_SETS = ("stcc",)  # with the Digital Sound Board
DSB2_SETS = ("topskatr",)  # with the Digital Sound Board 2 (a 68000)
M1SND_SETS = {"manxttc": "manxtt"}  # with a Model 1 sound board (its ROMs from the named set beside it)
# MAME parents whose archives may hold a clone's whole set (hotd.7z with hotdo's files, doa.zip with doab's:
# older romsets hold Revision B under the parent's name).
PARENTS = OTHER_NAMES
MODEL2A_SETS = MODEL2A_ONLY_SETS + MODEL2B_SETS + MODEL2C_SETS
# The i960 code those sets run from RAM copies of program ROM (source, RAM
# address, bytes): the recompiler reads that code from the ROM.
CODE_COPY = {"srallyc": [("0x1000", "0x5a0000", "0x60000")],
             # Over Rev: 128 KB of ROM at RAM 0x200000 and 512 KB at work RAM 0x500000
             "overrevb": [("0x60000", "0x200000", "0x20000"), ("0x80000", "0x500000", "0x80000")],
             # Gunblade NY: a 0x520-byte routine its setup (0x97160) copies to work RAM
             "gunblade": [("0x96d40", "0x532e50", "0x520")],
             # Zero Gunner: its boot (0x6a0) copies the program's upper 768 KB to work RAM
             "zerogun": [("0x40000", "0x540000", "0xc0000")],
             # Pilot Kids: the same, 704 KB from 0x50000 to 0x550000 (its boot at 0x6d0)
             "pltkids": [("0x50000", "0x550000", "0xb0000")],
             # Motor Raid: its boot (0x3ca00) copies 128 KB from 0xb0000 to RAM 0x200000
             "motoraid": [("0xb0000", "0x200000", "0x20000")],
             # Wave Runner: the same copy as Motor Raid, 128 KB from 0xb0000 to RAM 0x200000
             "waverunr": [("0xb0000", "0x200000", "0x20000")],
             # Super GT 24h: 16 routines its mode setup (0x56590-0x56df0) copies to RAM
             "sgt24h": [("0xbb0d0", "0x215400", "0x400"), ("0xbaf50", "0x215800", "0x400"),
                        ("0xbace0", "0x215c00", "0x400"), ("0xbaa80", "0x216000", "0x400"),
                        ("0x2ba30", "0x216400", "0x1800"), ("0x2cfd0", "0x217c00", "0x1c00"),
                        ("0x57fa0", "0x219800", "0x300"), ("0xb8ca0", "0x219b00", "0x900"),
                        ("0x21dd0", "0x21a400", "0xc00"), ("0x1ef00", "0x21b040", "0x2a0"),
                        ("0x1f910", "0x21b340", "0x290"), ("0x1f670", "0x21b640", "0x280"),
                        ("0x1f280", "0x21b8c0", "0x340"), ("0x582f0", "0x21bc40", "0x40"),
                        ("0x21180", "0x21bc80", "0x140"), ("0xbb550", "0x21be00", "0x200")]}
# Sets whose system procedure table is shorter than the i960's 260 entries:
# the words after it are not entries, so they are not seeds. Pilot Kids'
# (at 0x1f0) runs into the fault handlers' strings at 0x310 (entry 72); it
# never uses calls.
SYSTEM_PROCEDURES = {"pltkids": 72, "skisuprg": 16}


def run(cmd, **kw):
    print("+ " + " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True, cwd=ROOT, **kw)


def tool(build, config, name):
    """Path of a built tool: single-config generators put it in the build
    directory, Visual Studio in build/<config>/."""
    exe = name + (".exe" if os.name == "nt" else "")
    for p in (os.path.join(build, exe), os.path.join(build, config, exe)):
        if os.path.exists(p):
            return p
    sys.exit(f"recompile: {name} was not built in {build}")


# Model 1 sets (cmake/model1.cmake): their own pipeline, model1(); MODEL1_SETS from games.toml
M1_DSB_SETS = ("swa",)  # sets with the Digital Sound Board
M1_IO2_SETS = ("wingwar",)  # sets with the model1io2 I/O board (its own Z80 BIOS)


def model1(args, build, roms):
    """The Model 1 steps: images straight from the ROM set (m1rom), then the
    V60 (m1recomp), TGP (m2tgprecomp --model1) and sound 68000 (m2sndrecomp)
    code into the build's gen/."""
    if not roms:
        sys.exit(f"recompile: put your ROM set at roms/{args.set}.zip (or .7z) first")
    rom = roms[0]
    build_cmd = ["cmake", "--build", build, "--config", args.config]
    run(["cmake", "-S", ".", "-B", build, "-DCMAKE_BUILD_TYPE=" + args.config, "-DM2_APP=OFF", "-DM2_CORE=OFF",
         "-DM1_ROMSET=" + args.set, "-DM1_CORE=OFF"])
    run(build_cmd + ["--parallel", str(args.jobs), "--target", "m1rom", "m1recomp", "m2tgprecomp", "m2sndrecomp",
                     "dsbrecomp"])
    gen = os.path.join(build, "gen")
    os.makedirs(os.path.join(gen, args.set + "_snd"), exist_ok=True)
    tgp_bin, snd_bin = os.path.join(gen, "tgp_copro.bin"), os.path.join(gen, args.set + "_snd_prog.bin")
    m1rom = [tool(build, args.config, "m1rom"), rom]
    if subprocess.run(m1rom + ["tgp_copro", tgp_bin, "--set", args.set], cwd=ROOT).returncode:
        sys.exit(ROM_REJECTED_EXIT)  # m1rom printed which file is missing or wrong
    run(m1rom + ["m1audio", snd_bin, "--set", args.set])
    run([tool(build, args.config, "m2tgprecomp"), tgp_bin, os.path.join(gen, "m1_tgp.cpp"), "--model1"])
    run([tool(build, args.config, "m1recomp"), rom, os.path.join(gen, args.set + "_v60"), "--set", args.set,
         "--seeds", os.path.join("seeds", args.set + ".txt"), "--speculate"])
    run([tool(build, args.config, "m2sndrecomp"), snd_bin, os.path.join(gen, args.set + "_snd", "snd_gen.cpp"),
         "--seeds", os.path.join("seeds", args.set + "_snd.txt")])
    if args.set in M1_DSB_SETS:  # the Digital Sound Board's Z80
        dsb_bin = os.path.join(gen, args.set + "_dsb_prog.bin")
        os.makedirs(os.path.join(gen, args.set + "_dsb"), exist_ok=True)
        run(m1rom + ["dsb_cpu", dsb_bin, "--set", args.set])
        run([tool(build, args.config, "dsbrecomp"), dsb_bin, os.path.join(gen, args.set + "_dsb", "dsb_gen.cpp")])
    if args.set in M1_IO2_SETS:  # the I/O board's Z80
        io_bin = os.path.join(gen, args.set + "_io_bios.bin")
        os.makedirs(os.path.join(gen, args.set + "_io"), exist_ok=True)
        run(m1rom + ["io2_bios", io_bin, "--set", args.set])
        run([tool(build, args.config, "dsbrecomp"), "--io2", io_bin, os.path.join(gen, args.set + "_io", "io_gen.cpp")])
    if args.gen_only:
        return
    # picks up the generated sources
    run(["cmake", "-S", ".", "-B", build, "-DM1_CORE=" + ("ON" if args.core else "OFF")])
    targets = ["m1core"] if args.core else ["m1run", "m1native", "m1sndcheck", "m1replay", "m1traceinfo"]
    run(build_cmd + ["--parallel", str(args.jobs), "--target", *targets])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--set", default="daytona93", choices=[*M2_SETS, *MODEL1_SETS])
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--config", default="Release")
    ap.add_argument("--jobs", "-j", type=int, default=os.cpu_count() or 1, metavar="N",
                    help="files compiled at once (default: one per CPU thread)")
    ap.add_argument("--fast_inaccuracy", action="store_true",
                    help="recompiler speed options (i960 block bookkeeping and direct chunk chaining, TGP block counting): faster, "
                         "not interrupt-exact against MAME")
    ap.add_argument("--rom", metavar="PATH", help="the ROM set (.zip or .7z) if not at roms/<set>.zip")
    ap.add_argument("--core", action="store_true",
                    help="build only the game core library (m2core, common/include/afcore.h) instead of the game and tools")
    ap.add_argument("--gen-only", action="store_true",
                    help="stop once the game's code is generated into the build's gen/ (for a core cross-built "
                         "from it with -DM2_GEN_ROOT, such as an arm64 Linux one built on x86-64)")
    args = ap.parse_args()
    build = os.path.join(ROOT, args.build_dir)
    cache = os.path.join(build, "rom_cache", args.set)
    roms = [os.path.join(ROOT, "roms", name + "." + ext) for name in (args.set, *PARENTS.get(args.set, ()))
            for ext in ("zip", "7z")]
    roms = [r for r in roms if os.path.exists(r)]
    if args.rom:
        roms = [os.path.abspath(args.rom)]
    if args.set in MODEL1_SETS:
        model1(args, build, roms)
        return

    build_cmd = ["cmake", "--build", build, "--config", args.config]
    # the build directory is for one ROM set (CMake's M2_ROMSET)
    run(["cmake", "-S", ".", "-B", build, "-DCMAKE_BUILD_TYPE=" + args.config, "-DM2_ROMSET=" + args.set,
         "-DM2_CORE=" + ("ON" if args.core else "OFF"), "-DM2_APP=" + ("OFF" if args.gen_only else "ON")])
    samples = "samples.bin" if args.set in MODEL2A_SETS else "pcm1.bin"  # SCSP or MultiPCM sound board
    # Manx TT: its Model 1 sound board's ROMs are read from the parent set beside the ROM set
    m1_new = args.set in M1SND_SETS and not os.path.exists(os.path.join(cache, "m1_program.bin")) and any(
        os.path.exists(os.path.join(os.path.dirname(r), M1SND_SETS[args.set] + "." + ext)) for r in roms for ext in ("zip", "7z"))
    if m1_new or not all(os.path.exists(os.path.join(cache, f)) for f in ("tgp_program.bin", "sound_program.bin", samples)):
        if not roms:
            sys.exit(f"recompile: put your ROM set at roms/{args.set}.zip (or .7z) first")
        run(build_cmd + ["--target", "m2import"])
        # The first archive that holds the whole set (a clone's split .zip may
        # hold only its own files, a .7z beside it the whole set).
        if all(subprocess.run([tool(build, args.config, "m2import"), r, cache], cwd=ROOT).returncode for r in roms):
            sys.exit(ROM_REJECTED_EXIT)  # m2import printed which file is missing or wrong
    run(build_cmd + ["--parallel", str(args.jobs), "--target", "m2recomp", "m2tgprecomp", "m2sharcrecomp", "m2tgpx4recomp", "m2sndrecomp", "dsbrecomp"])

    gen = os.path.join(build, "gen", args.set)
    if os.path.isdir(gen):
        for f in os.listdir(gen):
            os.remove(os.path.join(gen, f))
    os.makedirs(gen, exist_ok=True)
    recomp = [tool(build, args.config, "m2recomp"), os.path.join(cache, "program.bin"), gen,
              "--seeds", os.path.join("seeds", args.set + ".txt")]
    hooks = os.path.join("seeds", args.set + "_hooks.txt")
    if os.path.exists(os.path.join(ROOT, hooks)):
        recomp += ["--hooks", hooks]
    if args.set in MODEL2A_SETS:
        recomp += ["--model2a"]
    if args.set in CODE_COPY:
        for c in CODE_COPY[args.set]:
            recomp += ["--code-copy", *c]
    if args.set in SYSTEM_PROCEDURES:
        recomp += ["--system-procedures", str(SYSTEM_PROCEDURES[args.set])]
    if args.fast_inaccuracy:
        recomp += ["--fast_inaccuracy"]
    run(recomp)
    if args.set in MODEL2B_SETS:  # the SHARC's upload (m2import's tgp_program.bin)
        sharc = os.path.join(build, "gen", args.set + "_sharc")
        os.makedirs(sharc, exist_ok=True)
        run([tool(build, args.config, "m2sharcrecomp"), os.path.join(cache, "tgp_program.bin"),
             os.path.join(sharc, "sharc_gen.cpp")])
    elif args.set in MODEL2C_SETS:  # the TGPx4's upload (m2import's tgp_program.bin)
        tgpx4 = os.path.join(build, "gen", args.set + "_tgpx4")
        os.makedirs(tgpx4, exist_ok=True)
        run([tool(build, args.config, "m2tgpx4recomp"), os.path.join(cache, "tgp_program.bin"),
             os.path.join(tgpx4, "tgpx4_gen.cpp")])
        if args.set in DSB2_SETS:
            dsb2 = os.path.join(build, "gen", args.set + "_dsb2")  # the Digital Sound Board 2's 68000
            dsb2_seeds = os.path.join("seeds", args.set + "_dsb2.txt")
            os.makedirs(dsb2, exist_ok=True)
            run([tool(build, args.config, "m2sndrecomp"), os.path.join(cache, "dsb_program.bin"),
                 os.path.join(dsb2, "dsb2_gen.cpp"), "--dsb2"]
                + (["--seeds", dsb2_seeds] if os.path.exists(os.path.join(ROOT, dsb2_seeds)) else []))
        if args.set in DSB_SETS:
            dsb = os.path.join(build, "gen", args.set + "_dsb")  # the Digital Sound Board's Z80
            os.makedirs(dsb, exist_ok=True)
            run([tool(build, args.config, "dsbrecomp"), os.path.join(cache, "dsb_program.bin"),
                 os.path.join(dsb, "dsb_gen.cpp")])
    else:
        tgp = os.path.join(build, "gen", args.set + "_tgp")
        os.makedirs(tgp, exist_ok=True)
        run([tool(build, args.config, "m2tgprecomp"), os.path.join(cache, "tgp_program.bin"),
             os.path.join(tgp, "tgp_gen.cpp")] + (["--fast_inaccuracy"] if args.fast_inaccuracy else []))

    if args.set in M1SND_SETS:  # Manx TT: the Model 1 sound board beside the SCSP (its engine sounds)
        m1snd = os.path.join(build, "gen", args.set + "_m1snd")
        m1snd_seeds = os.path.join("seeds", args.set + "_m1snd.txt")
        os.makedirs(m1snd, exist_ok=True)
        m1_program = os.path.join(cache, "m1_program.bin")
        if os.path.exists(m1_program):
            run([tool(build, args.config, "m2sndrecomp"), m1_program, os.path.join(m1snd, "m1snd_gen.cpp"), "--m1snd"]
                + (["--seeds", m1snd_seeds] if os.path.exists(os.path.join(ROOT, m1snd_seeds)) else []))
        else:  # its ROMs are in the parent set (manxtt), not found beside this one: no engine sounds
            print("recompile: no %s beside %s: building without its engine sounds" % (M1SND_SETS[args.set], args.set))
            with open(os.path.join(m1snd, "m1snd_gen.cpp"), "w") as f:
                f.write('#include "runtime/m1snd_gen_support.h"\n\nnamespace m1sndgen {\n\n'
                        "void run(Env &) {}\nbool has_code(uint32_t) { return false; }\n"
                        "uint64_t native_instructions() { return 0; }\n\n} // namespace m1sndgen\n")

    snd = os.path.join(build, "gen", args.set + "_snd")
    snd_seeds = os.path.join("seeds", args.set + "_snd.txt")
    os.makedirs(snd, exist_ok=True)
    run([tool(build, args.config, "m2sndrecomp"), os.path.join(cache, "sound_program.bin"),
         os.path.join(snd, "snd_gen.cpp")] + (["--model2a"] if args.set in MODEL2A_SETS else [])
        + (["--seeds", snd_seeds] if os.path.exists(os.path.join(ROOT, snd_seeds)) else []))

    if args.gen_only:
        return
    if args.core:
        run(["cmake", "-S", ".", "-B", build])  # picks up the generated sources
        run(build_cmd + ["--parallel", str(args.jobs), "--target", "m2core"])
        return
    run(["cmake", "-S", ".", "-B", build])  # picks up the generated sources
    run(build_cmd + ["--parallel", str(args.jobs)])  # the game (daytona, m2run) and the check tools


if __name__ == "__main__":
    main()
