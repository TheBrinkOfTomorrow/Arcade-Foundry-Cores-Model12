# Model 2 cores

How the Model 2 cores were planned and built: Sega's 1993-1998 3D board in
all four versions (Model 2, 2A, 2B and 2C), from Daytona USA to Dead or
Alive. The dated history is in [HANDOFF.md](../../HANDOFF.md) (October 2026
in [handoff/2026-10.md](../../handoff/2026-10.md)), the current state in
[STATE.md](../../STATE.md) and the rules in `rules.md`. The Model 1 cores
live in the same repository and share the build, the sound board and the
core library, but not the board; they have their own document,
[MODEL1.md](MODEL1.md). The first design, written for Daytona USA alone, is
kept as it was in
[daytona-usa-recomp-design.md](../daytona-usa-recomp-design.md): its
measurements still hold, its plans are what this document replaces.

## Purpose and scope

Play Sega's Model 2 arcade games natively in the app: the user supplies
their own MAME ROM sets, the build turns each set into a core, and the app
plays it with a gamepad, a wheel and pedals, a light gun, a flight stick,
twin sticks or a fight stick.

Four boards and 32 sets are built:

| Board | Sets | Geometry | Sound |
|---|---|---|---|
| Model 2 | daytona93, daytona, vcop, desert | TGP (MB86234) | Model 1 sound board: 68000, YM3438, two MultiPCMs |
| Model 2A | srallyc, manxttc, vcop2, vf2, motoraid, dynamcop, skytargt | TGP (MB86234) | SCSP board: 68000, SCSP |
| Model 2B | indy500, fvipers, sgt24h, overrevb, von, gunblade, vstriker, zerogun, pltkids, lastbrnx, schamp, dynabb97, rchase2, doab | SHARC (ADSP-21062) | SCSP board |
| Model 2C | stcc, hotdo, waverunr, segawski, skisuprg, bel, topskatr | TGPx4 (MB86235) | SCSP board |

Every board has the same main CPU, an Intel i960KB at 25 MHz, and the same
fixed-function geometrizer and rasterizer. A few sets add a board of their
own: Touring Car the Digital Sound Board (a Z80 and an MPEG decoder), Top
Skater the Digital Sound Board 2 (a 68000 and an MPEG decoder), Manx TT the
Model 1 sound board beside the SCSP, and Zero Gunner, Pilot Kids and
Dynamite Cop the 315-5881 security chip.

In scope: attract, play, test mode, sound, the operator settings and high
scores (the board's EEPROM and backup RAM), force feedback commands, link
play for the linked cabinets, and the app's library, Add ROM and controls on
every platform the app runs on. Daytona USA also has its enhancements
(widescreen, draw distance), and every set can be drawn in stereo for VR
headsets and 3D displays.

Not built: the Model 2 sets no one here owns (Air Walkers, Hyper Athlete,
Power Sled, Rascot 2), Manx TT's Deluxe set, and the rest of the list in the
app's GAMES.md.

## Requirements

1. Nothing from a ROM is in the repository: no program image, no generated
   C++, no samples or textures (`rules.md` 6 and 7). The programs are
   translated on the user's machine at build time.
2. Every set behaves as it does in MAME, which is the reference: attract,
   test mode and a full game to game over, checked instruction by
   instruction for each CPU and screen by screen, with the input scripts
   committed.
3. No emulator at run time. Every program a board runs (the i960's, the
   geometry processor's, the sound CPU's) is statically recompiled to C++;
   fixed-function hardware is native C++. The one exception is a fallback
   interpreter for i960 code no run reached, so a player who gets somewhere
   new keeps playing.
4. One core per set behind the shared `afcore.h` interface (from Common), so
   the app handles a Model 2 game like any other.
5. The user's MAME set is used as it comes, `.zip` or `.7z`, with a clone's
   parent set beside it; every file is checked by CRC before anything is
   built.

## How it was planned

The project started as a recompilation of Daytona USA for Windows, macOS
and Linux, from the
[daytona-arcade-recomp](https://github.com/alphanu1/daytona-arcade-recomp)
project, whose history this repository keeps. Its design doc set the method
the other 31 sets followed:

- **Recompile ahead of time.** The i960 program becomes C++
  (`tools/m2recomp`), and so does the program the i960 uploads to its
  geometry processor (`m2tgprecomp`, then `m2sharcrecomp` and
  `m2tgpx4recomp`) and the sound CPU's (`m2sndrecomp`). Nothing is
  interpreted in a matching run.
- **Model the board natively.** Memory maps, the geometrizer, the
  rasterizer, the System 24 tilemaps, the I/O chips, the sound chips and the
  link board come from MAME's drivers (BSD-3-Clause), transplanted into
  plain C++ without MAME's device framework.
- **Use MAME as the oracle.** A patched MAME writes the i960's device
  accesses and interrupts, the geometry processor's FIFOs, the sound CPU's
  device accesses and a hash per screen. The native tools replay those logs
  and stop at the first difference.
- **Parity before speed.** Optimisations came only after a set matched, and
  each one is checked against the reference build frame by frame.

Daytona went through milestones (boot to attract with i960 parity, then the
TGP and geometrizer, then pixels, then sound and a full race), and Sega
Rally followed the same steps on the 2A board. After that the order was one
board at a time, each new set through the same steps: the i960 in lockstep
over attract, the geometry processor, the screens, the sound CPU, then test
mode and a full game, then the core and the app entry. The 2A sets came
first, then the 2B sets (the SHARC), then the 2C sets (the TGPx4), with
Desert Tank and Virtua Cop filling in the original board.

## Design

### Overview

```
MAME set (.zip / .7z)
  │
  ├─ build time: m2import ── checks CRCs, writes the program and data images
  │              m2recomp ── i960 program to C++ (seeds + seed_scan)
  │              m2tgprecomp / m2sharcrecomp / m2tgpx4recomp ── geometry program to C++
  │              m2sndrecomp ── sound 68000 program to C++
  │
  └─ run time: afcore_<set>
        generated i960 code  ⇄  M2Board (memory map, interrupts, timers)
                                 geometry processor (generated) → buffer RAM
                                 geometrizer → rasterizer → 3D layer
                                 System 24 tilemaps + palette → screen
                                 sound board (generated 68000 + chips) → audio
                                 I/O chip, EEPROM, drive board, link board ← inputs
```

### Repository layout

| Path | What |
|---|---|
| `src/i960` | The i960 decoder (MAME's executor's view of the encodings), reachability, the FP operations |
| `src/refcore` | MAME's i960 interpreter, transplanted: the lockstep reference and the fallback interpreter |
| `src/runtime` | The board: `m2_board` (map, interrupts, timers, per-set constants), `game_loop`, `m2_tgp_board`, `tgp`, `sharc/`, `tgpx4/`, `geo` (geometrizer), `raster` (rasterizer), `video` (tilemaps, palette, composition), `sound_board` (Model 1 sound board), `scsp_board` and `scsp`, `dsb2`, `multipcm`, `io_315_5649`, `drive_board.h`, `comm_board`, `crypt5881/`, `rom_import`, `enhance`, `stereo`, `lockstep` |
| `src/m68k` | The 68000 decoder the sound recompiler uses |
| `src/core` | The core library: `m2core.cpp` (afcore), `board_inputs.h`, `audio_ring.h`, `core_versions.h`; `m1core.cpp` is Model 1's |
| `src/model1` | The Model 1 board ([MODEL1.md](MODEL1.md)); Model 2 uses its MPEG decoder and DSB |
| `tools/` | The recompilers, `m2import`, `m2run` (the board free-running), the lockstep harnesses (`m2native`, `m2replay`, `m2tgpcheck`, `m2sharccheck`, `m2tgpx4check`, `m2sndcheck`, `dsbcheck`, `dsb2check`), `m2coreshot` (the core through afcore), `i960dis` |
| `seeds/` | Per set: i960 entry points (`<set>.txt`), sound entry points (`<set>_snd.txt`), Daytona's enhancement hooks; addresses only |
| `scripts/` | `recompile.py` (the whole build), `seed_scan.py`, `gen_games.py`, `run_trace.sh`, `make_input.py`, the MAME transplant scripts; `scripts/inputs/` holds the input scripts |
| `patches/mame/` | The trace patches for the oracle build (0001-0005 and 0007-0010; 0006 is Model 1's) |
| `games.toml` | Every set: title, year, board, controls, core version |

### Building a core

`scripts/recompile.py --set SET --rom ROMSET` does the whole build, and the
app's `build_cores.py` drives it for every set and platform:

1. `m2import` reads the set (and its parent: Daytona USA's sets, Manx TT,
   Dead or Alive), checks every file's CRC against `rom_import.cpp`'s tables
   and writes the images the recompilers read, de-interleaved and
   byte-swapped. An old dump that MAME has since replaced is accepted where
   it was checked to run the same (Dead or Alive's mpr-19324).
2. `m2recomp` translates the i960 program from `seeds/<set>.txt`. Code a set
   copies into RAM and runs there (Sega Rally's whole program, Over Rev,
   Zero Gunner, Pilot Kids, Motor Raid, Wave Runner, routines in Gunblade
   NY and Super GT 24h) is recompiled at its RAM address from the ROM bytes
   (`CODE_COPY` in `recompile.py`).
3. The geometry program the i960 will upload is found in the program or
   data ROM and recompiled (`m2tgprecomp`, `m2sharcrecomp`,
   `m2tgpx4recomp`). At boot the core checks the uploaded words are the ones
   recompiled.
4. `m2sndrecomp` translates the sound 68000 program (and the DSB2's, and
   Manx TT's Model 1 sound board's) from its vectors and
   `seeds/<set>_snd.txt`.
5. CMake builds `afcore_<set>` (`-DM2_CORE=ON`): the generated code, the
   runtime and the core library. On Apple platforms it is a framework.

`M2_ROMSET` and `M2_BOARD` select the set and board at configure time;
`games.cmake` (generated from `games.toml`) gives each set's board, and
CMake adds `M2_DSB`, `M2_DSB2` and `M2_M1SND` for the sets with an extra
sound board. Cross builds (iOS, tvOS, arm64 Linux) run `--gen-only` on the
host and build the generated code with the target's toolchain. Everything
the build writes stays under `build/`, which git ignores.

### Seeds

The recompiler follows every direct branch and call; indirect ones (`bx`,
`callx`, jump tables, interrupt and fault tables, system procedures) need
their targets listed. A set's seeds come from three places:

- **MAME's harvest.** Patch 0001 logs every indirect branch target MAME
  executes, over attract, test mode and scripted games.
- **`scripts/seed_scan.py`.** Static analysis of what no run reached:
  function starts in the system procedure table, code pointers in data, and
  addresses after the alignment padding a function start follows. A pointer
  is kept only if everything reachable from it decodes cleanly. Two rules
  were added after players found gaps: a function start may follow up to
  three zero padding words (Virtua Cop, #17), and a data pointer may point
  just past a data table where the word before it is not an instruction
  (Fighting Vipers' coin assignment menu).
- **The fallback interpreter's log.** A shipped core that reaches code with
  no seed logs the address once; it becomes a seed at the next build.

Seeds are addresses only: our analysis of MAME's runs, no ROM bytes, so
they are committed.

### The i960

The recompiler emits one C++ label per instruction, with direct branches as
gotos and a dispatch switch for indirect targets. The register file, the
arithmetic controls and the four-frame local register cache are modelled as
MAME's `i960.cpp` does, because the stack bytes, and so the RAM hashes,
depend on them. Decoding follows MAME's executor, not its disassembler:
three encodings the two read differently are flagged, and a Ghidra SLEIGH
cross-check agrees on every reachable instruction.

Floating point: Daytona's reachable FP is only conversions and compares,
proven bit-identical to a SoftFloat extended-precision reference over every
32-bit input. Later sets use the rest of the i960KB's real-number
instructions; those follow MAME's host-double semantics, since MAME is the
oracle.

Interrupts are taken at instruction boundaries the generated code checks.
In lockstep runs (`m2native`) MAME's interrupt placement is replayed at the
same completed-instruction counts (`src/runtime/lockstep`); in the shipped
core the board raises them and they are taken at the next boundary.

**Fallback interpreter.** Since November 2026 a jump to an address with no
recompiled code runs MAME's interpreter (`src/refcore/i960_ref`) with the
generated code's bookkeeping until control is back in recompiled code or
the frame ends (`GameLoop::no_code`). Each entry address is logged once.
`AF_DEBUG_M2_STRICT=1` restores the hard stop for lockstep runs and seed hunting, and
`m2native` never falls back. Virtua Cop recompiled from a quarter of its
seeds runs the same frames, instruction counts and screens as the full
build.

### Geometry processors

The i960 uploads a program to its geometry processor at boot, pushes
commands into its input FIFO and reads results from its output FIFO; the
processor also writes polygon data into buffer RAM for the geometrizer. Each
board's processor runs MAME's instruction semantics, transplanted, with the
uploaded program recompiled so nothing is interpreted:

- **TGP (Model 2, 2A).** Fujitsu MB86234 (MAME's MB86233 core).
  `m2tgprecomp` emits one label per word with the semantics inlined from
  `src/runtime/tgp.h`. Daytona's program is 2,024 words from its data ROM;
  each set's program is found in its ROMs and recorded in the importer.
- **SHARC (Model 2B).** Analog Devices ADSP-21062 (`src/runtime/sharc`,
  `scripts/transplant_sharc.py`). `m2sharcrecomp` emits code that calls
  MAME's handlers with each opcode as a constant. Most sets boot it through
  the FIFO; Last Bronx uploads through the IOP's DMA.
- **TGPx4 (Model 2C).** Fujitsu MB86235 (`src/runtime/tgpx4`,
  `scripts/transplant_tgpx4.py`). `m2tgpx4recomp` runs MAME's execute step
  around each instruction; a pop from an empty FIFO stalls the instruction
  as MAME's does.

The geometry processor has no clock. It runs when the i960 needs it
(reading the output FIFO, its status or the mailbox at the end of buffer
RAM) until it waits for input. Lockstep showed this gives the game the same
results with fewer poll iterations; nothing else differs.

### Geometrizer, rasterizer and video

- **Geometrizer** (`src/runtime/geo.cpp`, MAME's `model2_v.cpp`). At
  vblank it walks the display list in buffer RAM: transforms, lights,
  culls, clips and projects the polygons. The geometrizer's own microcode
  was never dumped, so this is MAME's high-level version. Top Skater's
  "compact" object command is handled beyond MAME (`kCompactObjects`),
  which draws it wrong. Past MAME's limit of 32,768 polygons a frame the
  extra polygons are dropped and counted rather than stopping the game:
  Daytona's last frame before attract has a display list with no end code
  that loops back on itself (#16).
- **Rasterizer** (`src/runtime/raster.cpp`, MAME's renderer). Projects,
  orders and draws the polygons into a 512 x 512 RGB layer, with the
  textures, translucency, fog and luma tables. It is the ground truth: its
  frames match MAME's pixel for pixel.
- **Video** (`src/runtime/video.cpp`). The System 24 tilemaps (HUD, text,
  backgrounds), the palette and the composition of the 3D layer between
  them, at 496 x 384 and 57.52 Hz. The whole composed screen matches MAME's.

### Sound

- **Model 1 sound board** (Model 2, and Manx TT beside its SCSP;
  `src/runtime/sound_board`). 68000, YM3438 (ymfm) and two MultiPCMs
  (MAME's, transplanted), MAME's mix gains. The i960 sends command bytes
  through an i8251 UART at 31.25 kbit/s; its transmit interrupt must be
  modelled or no byte is sent.
- **SCSP board** (2A, 2B, 2C; `src/runtime/scsp_board`, `scsp`). 68000 and
  MAME's SCSP with its DSP, timers and DMA, 8 MB of samples. The UART
  bytes reach the SCSP's MIDI input.
- **Digital Sound Board** (Touring Car) and **DSB2** (Top Skater). Music
  and speech as MPEG layer II at 32 kHz, decoded by MAME's decoder; the
  DSB's Z80 and the DSB2's 68000 are recompiled.

No sound board has a clock. Time is counted in completed 68000 instructions
at the rate MAME runs that program (`kIps`), and events (a UART byte one
line-time after the last, a chip timer) land on that count; the chips are
rendered up to the count before each register write. Each sound CPU is held
to MAME's in lockstep (`m2sndcheck`, patches 0003 and 0004) over attract and
a game.

Manx TT's engine and road sounds come from the Model 1 sound board MAME
fits only to the Deluxe set. The Twin set's core builds that board too
(`M2_M1SND`), fed the same UART bytes, from the two ROMs in the parent set
`manxtt`; without the parent set the game runs with no engine sound.

The core hands the app one stream per board output at its own rate (the
FM/SCSP output, the MultiPCM output, the DSB's MPEG output) through a ring
buffer per stream that keeps the newest second when nobody reads. The
racing games mixed their SCSP output far below the others, which the
cabinet amplifier made up for, so the core raises Sega Rally, Super GT 24h,
INDY 500 and Rail Chase 2 to about the others' level, clamped
(`kOutputGain`).

### I/O, controls and the cabinet

- **I/O.** The original board talks to the Model 1 I/O board through a
  dual-port RAM (modelled from the game's traffic; Desert Tank's needs its
  latch bytes set or it drops into test mode). The 2A, 2B and 2C boards
  have the 315-5649 I/O chip (MAME's) with a 93C46 EEPROM on port A, with
  MAME's write and erase times.
- **Controls.** `src/core/board_inputs.h` maps the app's controls onto the
  board's ports as MAME's input ports define them: one switch table per
  cabinet type (wheel and gearbox, guns, turrets, fighting, flight stick,
  bikes, skis, jet ski), then the analogue inputs with MAME's ranges and
  reversals. Light-gun positions go through MAME's gun ranges; a gun aimed
  off screen reloads.
- **Drive board.** The force feedback commands (Daytona USA through the
  I/O board, Sega Rally through the 315-5649's port E) go to `drive_board.h`, modelled from the board's own
  program as checked in MAME: one effect at a time (spring, resistance,
  constant force), no rumble. The app turns them into wheel forces.
- **Protection.** The 315-5881 (Zero Gunner, Pilot Kids, Dynamite Cop) is
  MAME's decryption, transplanted (`src/runtime/crypt5881`). Dead or
  Alive's 317-0229 is handled as MAME's driver does.
- **Cabinet setup.** On first run with a blank EEPROM, Manx TT is set to a
  Twin cabinet through its test mode, so it does not wait for a motion seat.
  Sega Ski Super G stops at boot on DRIVE BOARD TROUBLE, as in MAME, so the
  core presses test once for the player while that screen shows.

### Link play

The link board (`src/runtime/comm_board`, MAME's m2comm in its simulation
mode) is shared RAM the cabinets pass round a ring. Each cabinet listens
for the one before it and connects to the next over TCP (`app::TcpLink`),
started by the core's `link_start`; frame synchronisation is optional. The
game's own test mode picks each cabinet's role. Daytona USA Revision A,
Sega Rally and INDY 500 are tested with several cabinets; Daytona USA '93
uses an older protocol that is not supported. The app's LINK_PLAY.md says
which games link and how to set them up.

### The core library

`src/core/m2core.cpp` implements `afcore.h`. It creates the game from the
user's set (or a folder `m2import` wrote), runs one frame per call, hands
over the screen and the audio streams, maps the inputs, saves the EEPROM
and backup RAM, starts link play, and draws the stereo views. `check_roms`
runs the importer's checks without building anything, so the app can say
which files are wrong. Each set's core is selected at build time and
exports one symbol, `afcore_api_<set>`; everything else is hidden, so one
process can hold several games.

A frame (`rt::GameLoop`) has no clock either: vblank starts when the game
waits in its wait-for-vblank loop (or after a frame's worth of i960 work if
it never does), and the frame ends when the vblank handler returns. On the
2A, 2B and 2C boards the board's timers run on board time kept in 25 MHz
ticks, because Sega Rally and later games run their game timing and send
their sound commands from them.

### Enhancements

All off by default; with them off the game is exactly as recompiled.

- **Daytona USA** (the desktop build; hooks the recompiler places at
  addresses in `seeds/daytona93_hooks.txt`): widescreen up to 21:9 (the
  side clip planes widened, the HUD left centred or moved to the screen
  edges), draw distance and the scenery budget, and separate music and
  effects volumes.
- **Stereo** (every set, `src/runtime/stereo`). The last screen's polygons
  drawn again for two eyes by the reference rasterizer, either as a pair
  either side of the game's camera or as a head-tracked view with the clip
  planes opened. It reads what the frame kept and changes nothing in the
  game. The app uses it for VR headsets and 3D displays.

### Speed

`M2_FAST_PATHS` (on everywhere since November 2026, #288 3d) adds the speed
work first done for the PS Vita port, all giving the same pixels and sound:
the rasterizer's and 2D layers' caches, the TGP's FIFOs as a ring, the bus
decoding the geometry ports first, the geometrizer's block reads, and the
3D drawn in bands on several threads (`AF_DEBUG_M2_RASTER_THREADS` sets how many).
Sega Rally on the x86-64 build host went from 82 to 158 frames a second.
`-DM2_FAST_PATHS=OFF` builds the reference, for lockstep runs and for
checking a fast path against it.

## Data and settings

- **Saves.** `ioboard_eeprom.bin` (the I/O board's or 93C46's EEPROM: the
  operator settings) and `backup_ram.bin` (scores, records) in the save
  directory the app gives the core.
- **Settings.** The game's own test mode holds the operator settings, as on
  the cabinet; the app passes the enhancement settings to the desktop build
  only.
- **Versions.** Each core has its own x.y.z version in `games.toml`
  (written into `core_versions.h` by `gen_games.py`). Major when old saves
  stop carrying over, minor for a change players see, patch for any other
  change to the core's code. The app refuses a replay made with another
  core version.

## Platforms

The cores are plain C++20 with no platform code, so they build wherever the
app runs: macOS, iOS, iPadOS and tvOS (frameworks), Linux x86-64 and arm64.
The app's `build_cores.py` builds them for each.

Measured speed: on a Raspberry Pi 5 Daytona USA runs at full speed (57.0
of 57.52 frames a second over a game, slowest second 55.9). On a Pi 4 it
averages 87%, at 88-95% in races, with the app's game thread.

The standalone desktop build (`M2_APP=ON`, SDL3, Windows, macOS, Linux) is
still in the tree, off by default. The PS Vita, Dreamcast, Android and iOS
(SDL) ports are archived on the `ports-archive` branch (tag
`ports-archive-2026-11-02`) and are not built.

## Testing

- **Against MAME.** For every set: attract, test mode with each item
  walked, and a full game to game over, with every i960 instruction,
  geometry output word, polygon and composed screen compared, and the sound
  CPU in lockstep. The runs, their lengths and their results are in the
  HANDOFF entry for each game. MAME's own known bugs are noted where they
  show (Sega Rally's enemy car placement, Top Skater's compact objects).
- **Regressions.** A change to shared code is checked on the other boards'
  sets: their screen hashes over 3,000 frames must be unchanged before and
  after.
- **Unit tests.** `ctest` runs 20 ROM-free tests (decoder, FP, 68000,
  geometrizer clipping and stream, MultiPCM, comm board, audio ring, frame
  pacing and others); two need MAME and skip without it.
- **In CI.** Pull requests get the release scan, `gen_games.py --check`
  and a ROM-free Linux build with the tests. The ROM runs happen in the
  app's nightly on a self-hosted runner.
- **In the app.** `m2coreshot SET FRAMES --hashes` runs a core through
  afcore and prints a hash per frame. Device results go into the app's
  compatibility table.

## Changes since the first design

What changed from the Daytona design, and why:

- **From one game to 32.** The board became a type beside the set
  (`M2_BOARD`) and the per-set differences constants in `m2_board.h`;
  the SHARC and TGPx4 got their own recompilers.
- **CPU reference rasterizer instead of a GPU renderer.** MAME's renderer,
  transplanted, matches MAME pixel for pixel and is fast enough everywhere
  with the fast paths; the app only shows the finished frame.
- **The front end moved to the app.** The cores became libraries behind
  `afcore.h` (renamed from `m2core.h` in October 2026, interface 5.0.0),
  and the interface header moved to Cores-Common.
- **A fallback interpreter after all.** The first design had none; after
  1.0.0 a player reaching an unseeded path lost the game, so the i960 now
  falls back to MAME's interpreter (#30).
- **Fast paths on everywhere** (#288 3d), after a release of device
  results.
- **One game list** (`games.toml`, #33), audio ring buffers (#36) and
  table-driven inputs (#35), all with no change to what a game does.
- **Native audio** (a sequencer-based replacement for Daytona's sound
  board, experimental) is used only by the desktop build; the cores play
  the recompiled sound board.

## Open items

- The sets not built yet: those no one here owns, Manx TT Deluxe, and the
  rest of the Model 2 list.
- The sound boards' clock is instructions, not 68000 cycles: MAME's rate
  varies with the instruction mix, so the board's time drifts from MAME's
  by tens of milliseconds.
- The geometrizer is MAME's high-level version; its real microcode is not
  dumped.
- `M2_DC_SPEED` code left from the Dreamcast port, to remove.
- Link play beyond the tested games (Manx TT, Motor Raid).

## Decisions

- **Static recompilation, MAME as the oracle only.** The cores never run
  MAME; an interpreter covers only code no run reached, and logs it.
- **MAME's semantics for every CPU, even where hardware may differ.** The
  i960's register cache, `addc`'s carry, the FP in host double: MAME is
  what we can compare against, so the cores follow it, and differences
  from hardware are noted, not guessed.
- **No clock for the geometry processor or the sound boards.** They run on
  demand or on instruction counts; lockstep proved the games see the same
  results.
- **Drop polygons past the limit instead of stopping** (#16).
- **Parity before speed.** No `-ffast-math`, no host doubles for TGP
  arithmetic without a bit-identical test, and every fast path checked
  against `-DM2_FAST_PATHS=OFF`.
- **No hand edits to generated code.** A problem is fixed in the
  recompiler or by adding a seed.
- **Seeds are addresses only.** They are committed; traces, generated code
  and frame dumps are not.
