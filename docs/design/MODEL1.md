# Model 1 cores

How the Model 1 cores were planned and built: Sega's first 3D board
(1992-1994), Virtua Racing, Star Wars Arcade, Virtua Fighter and Wing War.
They live beside the Model 2 cores and share their build, recompilers,
sound board and core library; the board itself is different, so this is a
document of its own. The Model 2 design is [MODEL2.md](MODEL2.md). The
dated history is in [HANDOFF.md](../../HANDOFF.md) (October 2026 in
[handoff/2026-10.md](../../handoff/2026-10.md)), the current state in
[STATE.md](../../STATE.md) and the rules in `rules.md`.

## Purpose and scope

Play the Model 1 games natively in the app, on the same terms as Model 2:
the user's own MAME ROM sets, one core per set, the app's controls.

| Set | Game | Controls | Extra boards |
|---|---|---|---|
| vr | Virtua Racing | wheel, pedals, 4 view buttons, shifter | link board |
| vformula | Virtua Formula | as Virtua Racing | link board (simulated, so its own Z80 program epr-15624.17 is not loaded); no motion seats or camera feeds |
| swa | Star Wars Arcade | flight stick | Digital Sound Board |
| vf | Virtua Fighter | two players, three buttons and a stick | I/O board with the B BIOS |
| wingwar | Wing War | flight stick, throttle | model1io2 I/O board (its Z80 runs) |

In scope: attract, play, test mode, sound, the operator settings and high
scores, link play (Virtua Racing, Virtua Formula), and the app's library and controls on
every platform, and stereo views for VR headsets and 3D displays
(`src/model1/stereo.cpp`, the same idea as Model 2's).

## Requirements

The same as Model 2's ([MODEL2.md](MODEL2.md#requirements)): nothing from a
ROM in the repository, MAME as the reference for attract, test mode and a
full game, every program recompiled ahead of time with an interpreter only
for code no run reached, one core per set behind `afcore.h`, the user's set
checked by CRC.

## How it was planned

Model 1 came after the Model 2 queue, in the order Virtua Racing, Star Wars
Arcade, Virtua Fighter, then Wing War (close to Star Wars Arcade). The new
work was a V60 recompiler, the Model 1 renderer and the TGP's FIFO path;
the sound board was already Daytona USA's, and the TGP's instruction
semantics already Model 2's. MAME's Virtua Fighter has a known collision
bug in its TGP RAM port and its Star Wars Arcade imperfect graphics, so
those were checked against what MAME gets right.

## Design

### The board (from MAME `model1.cpp`)

| Part | Detail |
| --- | --- |
| Main CPU | NEC V60 at 16 MHz. MAME charges 8 cycles an instruction: about 2 M instructions a second (58.56 M in 30 s of attract). |
| Memory (24-bit) | ROM 000000-0fffff and 200000-2fffff; data ROM window 100000-1fffff (bank register at e00004: low nibble 1 selects `0x1000000 + 0x100000 * ((d >> 4) & 7)`); ROM f80000-ffffff (reset at fffff0). RAM: 400000 backup 64 KiB, 500000 work 256 KiB, 600000 display lists, 680000 list control, 700000 tiles, 780000 characters, 900000 palette, 910000 colour translation. |
| Devices | b00000 comm board, c00000 I/O board dual-port RAM, c40000 sound UART, d00000-dfffff TGP coprocessor (also in I/O space), e00000 interrupt controller, e00002 mask, e00004 bank, e00006-e0000f timers. |
| Interrupts | vblank on level 1 at scanline 384; the vector is read at `(SBR & ~0xfff) + (irq + 0x40) * 4`. |

### The V60

`scripts/transplant_v60.py` turns MAME's V60 core into `src/v60/mame/*.inc`:
every op handler becomes a member template of `v60::Core` on a hook class
that does the operand decoding.

- **`tools/m1recomp`** runs each instruction's handler at recompile time
  with recording hooks that evaluate the addressing modes over symbolic
  values (`sym.h`, `sym_am.inc`). The result is a hook class per
  instruction with every operand resolved, and the instruction's length.
  Successors come from running it under 64 machine states (flags, registers
  and memory varied); more than two means an indirect transfer. Output: one
  chunk of 1,500 instructions per function and file under
  `build/gen/<set>_v60/` (never committed); a transfer inside a chunk is a
  goto, any other goes back to `run_native`, which finds the chunk by
  address. A successor the analysis did not see goes through the chunk's
  dispatch switch.
- **Seeds.** The worklist starts at the reset vector plus `seeds/<set>.txt`:
  MAME's executed addresses over attract, races or fights, game over, name
  entry, every test menu and linked cabinets, reduced to the fewest that
  reach the same code (`--reduce`), plus `--speculate` (what follows a
  return or jump, and what ROM words point at).
- **Code in RAM.** Wing War runs routines from backup RAM
  (0x40d000-0x40dfff), copied there by mode. `RomSet::code_copies` lists
  the ROM ranges copied; m1recomp analyses each copy over a bus whose RAM
  pages hold that copy and keys its instructions `pc | (copy + 1) << 24`.
  `run_native` finds the key with `key_of()`, which compares RAM against
  each copy's bytes. The data ROM window is filled from bank 0 at recompile
  time, since code runs there.
- **Fallback interpreter.** `v60::Interp` (`src/v60/interp`), the hooks
  calling MAME's decoders at run time, is the reference for the tools and
  the game's fallback: `Board::FallbackEnv::miss` runs it until control is
  back at recompiled code (m1recomp's `has_native`), the run's limit, or a
  pending interrupt the PSW lets in. Each new entry address is logged once;
  `AF_DEBUG_M2_STRICT=1` restores the hard stop, and `m1native` never falls back.
  Virtua Fighter and Wing War from thinned seeds run identically, with 22.7
  M and 29.8 M instructions interpreted.

Every hook stores each field it writes, even one already holding that value
at recompile time: at run time it holds what the previous instruction left.

### The TGP

The TGP (MB86233) runs the 315-5573 program ROM, recompiled by
`m2tgprecomp --model1` with Model 2's instruction semantics (`rt::Tgp`) and
Model 1's board (`src/model1/tgp.h`, from MAME `model1_m.cpp`): FIFOs in
data space (0x100 in, 0x400 out), so every data read checks for a stall;
four copro RAM address and data ports, table ports and a data ROM window in
I/O space. `m1::Copro` is the V60 side (D00000 RAM address, D20000 RAM
data, D80000 FIFO, DC0000 status). Wing War's TGP arithmetic needs 128-bit
integers (`__int128`), which MSVC lacks; clang-cl builds the tree.

### Video

`src/model1/video.cpp` is MAME's `model1_v.cpp`: at each screen update it
draws the System 24 background layers, walks the display list the V60 built
(600000 or 610000, chosen by the list control at 680000) into transformed,
lit, clipped and depth-sorted flat quads, then the HUD layers and the
above-HUD objects. At vblank, a frame that was not drawn still takes the
list's colour and object uploads, and the double buffer flips.
`M1_FAST_PATHS` (on everywhere) adds exact speed-ups to this path.

### The I/O board

- **EPR-14869 (Virtua Racing, Star Wars Arcade).** `src/model1/io_board.cpp`
  is the board's side of the dual-port RAM at C00000, modelled from the
  game's traffic; the Z80 and its BIOS are not run. At reset the settings
  EEPROM is in bytes 0x100-0x17f. Command 1 at byte 0x20 latches the ADC
  (bytes 0-7) and the switches (8-14); 2 and 3 store and load the EEPROM.
  Byte 0x11 is the drive board's command. Timed (as the core runs it), a
  latch clears 7.5 ms after the game writes it and a store 35.6 ms after;
  the data is there at once. Measured under MAME by the game's polls (2026-11-08):
  a game polls a latch only in test mode and after it (Virtua Formula:
  6,412 and 7,066 polls, 7.49 and 7.51 ms, the BIOS loop's phase not
  modelled), and a store on a blank EEPROM's first boot or a test-mode save
  (33,539 polls, 35.6 ms; the save 33,778).
- **EPR-14869B (Virtua Fighter).** The B BIOS's timing is visible to the
  game, so `RomSet::io_bios_b` switches `IoBoard` to a timed model of its
  Z80 loop, each byte landing on the dual-port RAM at the time MAME's Z80
  writes it (constants measured on MAME).
- **model1io2 (Wing War).** The advanced 837-10859 board, BIOS EPR-16891
  from the device set `model1io2`. Here the Z80 does run: `dsbrecomp --io2`
  recompiles its BIOS into `gen/<set>_io/io_gen.cpp`, driving `IoBoard2`
  (MAME's 315-5338A, MSM6253, 93C46 and CTC as plain members). Before each
  V60 access the Z80 runs up to the start of the V60's MAME timeslice.

### Sound

The sound board is Daytona USA's (`src/runtime/sound_board.cpp`: 68000,
YM3438, two MultiPCMs), with each set's program recompiled by
`m2sndrecomp` from `seeds/<set>_snd.txt` and its own instruction rate
(`RomSet::sound_ips`: vr 1,041,300 a second, swa 762,760). The V60 only
writes the UART at C40000.

Star Wars Arcade's music and speech come from the Digital Sound Board
(`src/model1/dsb.{h,cpp}`, after MAME's `dsbz80.cpp`): a Z80 that listens
to the sound 68000's UART and plays MPEG layer II streams from the board's
8 MB of ROM through MAME's decoder (`src/model1/mpeg_audio.cpp`) at 32 kHz.
`tools/dsbrecomp` recompiles all 391 of its instructions. As in MAME the
decoder runs only when the stream updates, 50 times a second of board
time. The core gives its output as audio stream 2. Model 2's Touring Car
and Top Skater use this decoder too.

### Timing

`src/model1/board.cpp` runs the board a frame at a time on MAME's clock:
the V60 at 8 cycles an instruction plus the cycles it waits, in 80 MHz
ticks. A frame is 278,144 V60 cycles (656 x 424 pixels at 16 MHz): the
vblank interrupt (level 1, vector 0x41) at line 384, the screen at line 0.
What makes the time MAME's:

- **The TGP, timed.** 40 MHz / 3, which MAME rounds up to 13,333,334 Hz, a
  cycle an instruction and two for the ALU ops MAME's mb86233 charges two.
  In each of MAME's timeslices the V60 runs first, so it sees the output
  words of the TGP instructions that started a cycle or more before the
  slice's start; a read from an empty FIFO syncs the TGP to the V60, costs
  a retry and, if still empty, halts the V60 until the TGP pushes.
- **The timers** (E00008/A, level 0): Star Wars Arcade's UART queue pump,
  Virtua Fighter's 128 us tick. Each tick ends a slice.
- **Interrupts.** MAME checks for one after every V60 instruction, so the
  generated code returns as soon as one is pending and enabled.
- **The I/O board's wait state** and its BIOS start-up. Believed until
  2026-11-08: the game's first command completes 2,507,400 cycles after
  reset, about nine frames in (measured on Virtua Racing's boot). It holds
  only for a game that writes its first command at that boot's time. MAME's
  polls of the first command (Virtua Racing 36,307; Virtua Formula 36,166
  from a blank NVRAM, 36,109 from a saved one, where the game writes it
  2.07 ms sooner) put the three ready times 1 and 8 passes of 267 us apart:
  the BIOS takes the command at its next main-loop pass and completes it
  3,067,035 ticks (38.3 ms) later (`IoBoard::kPass`, `kPassPhase`,
  `kReadyAfter`; the phase is the middle of the 57 us window the three
  writes leave). A boot from saved NVRAM was a frame late against MAME
  before; it is now identical.

With them, `m1run` free-running matches MAME's screens: Virtua Racing's 30
s of attract, 1,725 of 1,725 identical, and its scripted race, 7,000 of
7,000; Virtua Formula's in the section below.

### Link play

`src/model1/comm_board.{h,cpp}` is MAME's m1comm in its simulation mode:
shared RAM at B00000-B00FFF, CN at B01000, FG at B01002, ticked at line 384
before the vblank interrupt. Its protocol is not Model 2's: byte 1 is the
cabinet's role (1 master, 2 slave, 0 relay), the master numbers the ring
232 frames after both cables are up, and every frame each ready cabinet
sends its 0x1c4-byte block. The bytes go over `app::TcpLink`, as for
Model 2. Each cabinet's role is the test menu's GAME SYSTEM, LINK ID (NO
LINK, LIVE, SLAVE, MASTER), saved in the I/O board's EEPROM. Two cabinets
show MAME's screens up to the frame the link comes up, then race each other
as two linked MAMEs do; after that the link's timing is the host's. Virtua
Racing is tested with two cabinets in the app and four in `m1run`.
With frame sync on, the master may run up to `rt::link_slack` frames ahead
of its vsync coming back round the ring (cores 1.3.0; see MODEL2.md, Link
play).

### The core library

`src/core/m1core.cpp` puts the board behind `afcore.h` as `afcore_<set>`
(CMake `M1_CORE`, `cmake/model1.cmake`), like Model 2's: create, a frame
per call, the screen, the audio streams, saves, link play and `check_roms`.
Controls: the wheel and pedals 0x01-0xff, `AFCORE_SHIFT_UP` /
`AFCORE_SHIFT_DOWN`, or a change of `afcore_input.gear` held for four
frames as a shift; the flight stick and throttle for Star Wars Arcade and
Wing War; two players' buttons and sticks for Virtua Fighter.

### Save states

afcore 5.4's `state_size`, `save_state` and `load_state`, as on Model 2
(MODEL2.md, Save states: the same `rt::StateIO` pass and
`tests/save_state_check.sh`, with `m1coreshot`). `m1::Board::state` saves,
between frames: the V60's registers and flags (the rest of `v60::Core` is
an instruction's scratch); the board's clock (instructions and TGP stall
ticks), interrupt controller, timers and UART; the RAM regions and the data
ROM bank, mapped again on load; the TGP with its FIFOs, RAM and timing; the
video's view (light parameters, viewport, matrices), its object and colour
RAMs, list control and the screen as shown (the tile layers are built again
from tile and character RAM); the I/O board, or Wing War's with its Z80,
CTC and 93C46; the comm board with no link; the sound board (shared with
Model 2's original board) and Star Wars Arcade's DSB. The core adds its own
gear state (the last gear asked for and a shift under way) in an `AFM1`
chunk beside the board's `M1  `. No states during link play.

Checked on 11 Nov 2026 on an M4 Pro (64 GB, macOS, arm64), with
`tests/save_state_check.sh BUILD SET 1500 2100` at 7098557 (the code merged
as 2bf51c6). vr, swa, vf and wingwar each gave the same screen and sound on
every one of the 600 frames after the save, both in the run that saved and in
the run that loaded. A state is about 20 MB raw (vr 20,435,954 bytes, swa
20,535,404, vf 20,435,437, wingwar 20,501,040).

## Data and settings

- **Saves.** `ioboard_eeprom.bin` (the settings) and `backup_ram.bin`
  (400000, 64 KiB).
- **Versions.** Each core's x.y.z in `games.toml`, as for Model 2.

## Platforms

As Model 2: plain C++20, built by the app for macOS, iOS, iPadOS, tvOS and
Linux x86-64 and arm64. Windows with MSVC is not possible because of the
TGP's `__int128` (clang-cl works).

## Testing

- **Against MAME.** Patch 0006 writes the V60's lockstep trace
  (`M1TRACE_LOG`: the register file, every device access with the
  instruction count, each interrupt, RAM hashes at each interrupt
  controller write), the executed addresses (`M1TRACE_PCS`) and the frame
  log (`M1TRACE_FRAMES`: each vblank and screen update with three hashes).
  Input scripts drive both MAME (`scripts/m1_inputs.lua`) and `m1run
  --inputs`; MAME applies a switch at its next input update, so `m1run`
  applies switches a frame after the script's frame.
- **The sound CPU** is held to MAME's log by `m1sndcheck` (patch 0003),
  the Digital Sound Board by `dsbcheck` (120 s: all 10,669 reads and writes
  MAME's, within 45 us), Wing War's I/O board by `m1io2check`.
- **Save states** as for Model 2 (`tests/save_state_check.sh` with an
  `M1_CORE` build).
- **Regressions, unit tests and CI** as for Model 2.

| Tool | Checks |
| --- | --- |
| `m1replay ROMS TRACE` | The interpreter against the trace: every device access, interrupt and sample. |
| `m1native ROMS TRACE [--check] [--frames F] [--inputs S]` | The recompiled code against the trace; `--check` steps the interpreter beside it; `--frames` compares screens. |
| `m1recomp ROMS OUTDIR --seeds S [--pcs F...] [--reduce OUT] [--speculate]` | Generates the native code. |
| `m1run ROMS FRAMES [...]` | The board free-running (the core's code); `--frames` against MAME's; `--nvram`/`--save-nvram`; `--link-listen`/`--link-next`. |
| `m1rom ROMS REGION OUT` | One de-interleaved region (for `m2tgprecomp --model1`, `m2sndrecomp`, `dsbrecomp`). |
| `dsbrecomp`, `dsbcheck` | The Digital Sound Board's Z80 as C++, and against MAME. |
| `m1traceinfo TRACE` | Device accesses in a trace, grouped or listed. |

## Open items

- The sound board's clock is instructions, not cycles (as on Model 2).
- Link play for the other games' linked cabinets is not tested.

## Decisions

- **Model 1 is its own board, not a Model 2 variant.** Different CPU,
  geometry path and renderer; only the sound board, the TGP's instruction
  semantics, the MPEG decoder, the link transport and the core library are
  shared.
- **Recompile the V60 by running MAME's handlers at recompile time.** One
  definition of each instruction, with the operands resolved ahead of time,
  instead of a second hand-written decoder.
- **MAME's clock, not a clockless loop.** Unlike Model 2, the V60's
  timing against the TGP and the timers is visible to the games, so the
  board keeps MAME's cycle counts.
- **Run Wing War's I/O Z80, model the others.** Its board's behaviour is
  too timing-dependent to model from traffic; the others are simple
  enough.

## Virtua Formula

Sega's 1993 attraction version of Virtua Racing (MAME's `vformula`, a clone of
`vr`): up to eight linked sit-down cockpits on motion bases, with camera feeds
to the venue's screens. Its own program ROMs (epr-15638/15639 at 0x200000,
epr-15623 at 0xfc0000, epr-15622 at 0xfe0000, epr-15641/15640 at 0x1000000)
replace vr's; graphics, TGP, polygon and sound ROMs are vr's, read from the
merged `vr.7z`/`vr.zip` (games.toml `other_names`). epr-15624.17 is the comm
board's Z80 program (MAME loads it as the M1COMM BIOS `epr15624`; vr's is
`epr15112`). The comm board is simulated (`src/model1/comm_board.h`), so it is
not loaded. MAME emulates neither the motion seats nor the camera switching,
and neither does this core: the game boots, races, links and runs its test
mode without them (the input test shows EMERGENCY-SW and the CCD inputs as off).

Seeds: `seeds/vformula.txt`, 282 entries, `m1recomp --reduce` over MAME
harvests (`M1TRACE_PCS`) merged with the 138 the fallback interpreter had
logged before there was a MAME: attract (9,000 frames), a race to GAME OVER
(`scripts/inputs/vformula_game.txt`) and with the manual gearbox
(`vformula_manual.txt`), every test menu (`vformula_test.txt`), TGP TEST
(`vformula_tgp.txt`, which cannot be stopped), Bay Bridge and Acropolis
(`vformula_medium.txt`, `vformula_expert.txt`), name entry
(`vformula_name.txt`) and two MAMEs linked (`vformula_link_*.txt`, MAME's
`-comm_localport`/`-comm_remoteport`/`-comm_framesync`). Medium, Expert and
name entry need pokes (`scripts/m1_pokes.lua`): the race time is the 16-bit
word at 50116c in 1/32 s (1920 holds 60 s), and the player's lap the byte at
400eb5 (0-based; Beginner is 5 laps, so 4 is the last). With them every run
below is all native. `seeds/vformula_snd.txt` (vr's) needed nothing more:
`m1sndcheck` MATCH against MAME's sound log over the race (145.9 M
instructions, 4,734 interrupts), 9,000 frames of attract (164.8 M) and the
test mode with its sound test (208.6 M). `sound_ips` 1,042,260 (145.92 M
instructions in the race's 140 s; attract gives 1,049,437, vr's race
1,041,290 on the same MAME, so vr's 1,041,300 holds).

No EEPROM image: MAME's `vformula` ROM_START has no `ioboard:eeprom`
region, so its 93C46 starts blank, and the game's first boot writes its
defaults (a store, about two frames). The ROM set first loaded vr's
`93c45.bin`; with it the boot left MAME at frame 8 and the attract ran up
to 4 frames apart (1,538 of 1,800 screens within 4 frames, 8 identical).
Without it, and with the I/O board's store and latch timed (above), the
board matches MAME.

Checked against MAME frame by frame (`m1run --frames`, MAME `m1` built by
`scripts/build_mame.sh m1` at dddd7368 with `patches/mame`), on an M4 Pro
(64 GB, shared; 1-minute load 2 to 10 during the runs), 8 Nov 2026:

| Run (scripts/inputs) | Frames | Identical to MAME |
| --- | --- | --- |
| attract | 9,000 | 9,000 |
| `vformula_game.txt`: Beginner, automatic, time runs out, GAME OVER, ranking, attract | 8,000 | 8,000 |
| `vformula_manual.txt`: Beginner, manual, GAME OVER | 8,000 | 8,000 |
| `vformula_test.txt`: every test menu, a save, memory test, EXIT, attract | 11,400 | 11,398 |
| `vformula_tgp.txt`: TGP TEST | 1,800 | 1,800 |
| `vformula_link_master.txt`, `_slave.txt`: settings saved | 1,500 each | 1,500 each |
| attract from the test run's saved NVRAM | 1,800 | 1,800 |

The two test-mode frames are the brake released in the input test (MAME
applies analogue controls at its frame callback). `m1native` over MAME's
lockstep trace of the test run: MATCH, 397.8 M instructions, all 11,447
screens identical at every stage. Two of our cabinets linked on localhost
(`m1run --link-sync`, from MAME's saved NVRAM) race each other to GAME OVER;
against two linked MAMEs the master is identical to frame 1,060 and the
slave to 241, then the comm board's timing parts them (as for Virtua
Racing; two linked MAMEs differ from each other too). Virtua Racing on the
same build: attract 1,800/1,800, race 7,000/7,000, test 4,000/4,000. The
core (`afcore_vformula`): `m1coreshot` 1,800 frames of attract, all MAME's,
622 frames a second.

Settings (EEPROM, the same layout as vr's): LINK ID at 0x0C (0 NO LINK, 1
MASTER, 2 SLAVE; LIVE also offered), CAR NUMBER at 0x22 (0-based, eight cars);
COURSE SELECT defaults to MAJORITY VOTE and ADVERTISE SOUND is offered (vr has
neither). Test mode: the settings page saves only from EXIT with green, then
YES at "SAVE THE CHANGE?".

Before MAME (8 Nov 2026, M4 Pro 64 GB, m1run, about 300 fps): attract, a scripted
race to the finish, test mode, and three linked cabinets on localhost with frame
sync (1st/2nd/3rd of 3, the cars in their CAR NUMBER colours).
