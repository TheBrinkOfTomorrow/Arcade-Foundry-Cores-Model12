# Model 1 cores

Sega Model 1 games (Virtua Racing first, then Star Wars Arcade and Virtua
Fighter) built the same way as the Model 2 ones: the main CPU's program
statically recompiled to C++, the board's devices native, MAME as the
oracle in lockstep.

## Board (from MAME `model1.cpp`)

| Part | Detail |
| --- | --- |
| Main CPU | NEC V60 at 16 MHz. MAME charges 8 cycles an instruction: about 2 M instructions a second (58.56 M in 30 s of attract). |
| Memory (24-bit) | ROM 000000-0fffff and 200000-2fffff; data ROM window 100000-1fffff (bank register at e00004: low nibble 1 selects `0x1000000 + 0x100000 * ((d >> 4) & 7)`); ROM f80000-ffffff (reset at fffff0). RAM: 400000 backup 64 KiB, 500000 work 256 KiB, 600000 display lists, 680000 list control, 700000 tiles, 780000 characters, 900000 palette, 910000 colour translation. |
| Devices | b00000 comm board, c00000 I/O board dual-port RAM, c40000 sound UART, d00000-dfffff TGP coprocessor (also in I/O space), e00000 interrupt controller, e00002 mask, e00004 bank, e00006-e0000f timers. |
| Interrupts | vblank on level 1 at scanline 384; the vector is read at `(SBR & ~0xfff) + (irq + 0x40) * 4`. |

## Trace (patch 0006)

`M1TRACE_LOG=FILE` writes the V60's lockstep trace (`src/model1/trace.h`):
the register file at the first instruction, every device access (not RAM,
not ROM) with the instruction count, each interrupt, and at each write to
the interrupt controller the registers and a hash of every RAM region.
`M1TRACE_PCS=FILE` writes every executed address at exit. Clear the NVRAM
directory before a run, or the backup RAM hash differs.

    scripts/build_mame.sh m1
    rm -rf /tmp/nv; M1TRACE_LOG=vr.m1tr M1TRACE_PCS=vr_pcs.txt extern/mame/m1 vr \
        -rompath ROMS -nvram_directory /tmp/nv -video none -sound none \
        -nothrottle -seconds_to_run 30 -skip_gameinfo

## V60

`scripts/transplant_v60.py` turns MAME's V60 core into `src/v60/mame/*.inc`:
every op handler becomes a member template of `v60::Core` on a hook class
that does the operand decoding.

- `v60::Interp` (`src/v60/interp`): the hooks call MAME's decoders at run
  time. The reference interpreter, for tools only.
- `tools/m1recomp`: runs each instruction's handler at recompile time with
  recording hooks that evaluate the addressing modes over symbolic values
  (`sym.h`, `sym_am.inc`). The result is a hook class per instruction with
  every operand resolved, and the instruction's length. Successors come
  from running it under 64 machine states (flags, registers and memory
  varied); more than two means an indirect transfer. Output: one chunk of
  1,500 instructions per function and file under `build/gen/<set>_v60/`
  (never committed); a transfer inside a chunk is a goto, any other goes
  back to `run_native`, which finds the chunk by address. A successor the
  analysis did not see goes through the chunk's dispatch switch. Control
  reaching an address with no code is a hard error naming it.

The worklist starts at the reset vector plus the trace's executed
addresses (`--pcs`). Every hook stores each field it writes, even one
already holding that value at recompile time: at run time it holds what
the previous instruction left.

Code copied into RAM: Wing War runs routines from backup RAM
(0x40d000-0x40dfff), copied there by mode. `RomSet::code_copies` lists the
ROM ranges copied; m1recomp analyses each copy over a bus whose RAM pages
hold that copy and keys its instructions `pc | (copy + 1) << 24`, a chunk
holding one copy's code. `run_native` finds the key with `key_of()`, which
compares RAM against each copy's bytes. The data ROM window
(100000-1FFFFF) is filled from bank 0 at recompile time, since code runs
there (an RSR at 0x10f654).

## TGP

The TGP (MB86233) runs the 315-5573 program ROM, recompiled by
`m2tgprecomp --model1` with rt::Tgp's instruction semantics (Model 2's
TGP) and Model 1's board (`src/model1/tgp.h`, from MAME `model1_m.cpp`):
FIFOs in data space (0x100 in, 0x400 out), so every data read checks for a
stall; four copro RAM address and data ports, table ports and a data ROM
window in I/O space; nothing in the register file. `m1::Copro` is the V60
side (D00000 RAM address, D20000 RAM data, D80000 FIFO, DC0000 status). The
TGP runs when the V60 reads its FIFO or the copro RAM, until it waits for
input.

    m1rom ROMS tgp_copro build/gen/tgp_copro.bin
    m2tgprecomp build/gen/tgp_copro.bin build/gen/m1_tgp.cpp --model1

With `gen/m1_tgp.cpp` present, `m1native` runs the board natively against
the trace (`M1_REPLAY_TGP=1` replays it instead): every FIFO word must be
MAME's. Copro RAM reads are measured: the game polls a word the TGP holds
at FFFFFFFF while it works, and ours has always finished by then.

## Video

`src/model1/video.cpp` is MAME's `model1_v.cpp`: at each screen update it
draws the System 24 background layers, walks the display list the V60
built (600000 or 610000, chosen by the list control at 680000) into
transformed, lit, clipped and depth-sorted flat quads, then the HUD layers
and the above-HUD objects. At vblank, a frame that was not drawn still
takes the list's colour and object uploads, and the double buffer flips.

MAME's frame log (`M1TRACE_FRAMES`, patch 0006) gives each vblank and
screen update with the V60 instruction count and three hashes; `m1native
--frames` runs ours at the same instants and compares them:

    M1TRACE_LOG=vr.m1tr M1TRACE_FRAMES=vr_frames.txt extern/mame/m1 vr ...
    m1native ROMS vr.m1tr --frames vr_frames.txt [--dump DIR --every 120]

`scripts/rgb2png.py FILE 496 OUT.png` turns a dumped frame into a picture.

## I/O board

`src/model1/io_board.cpp` is the board's side of the dual-port RAM at
C00000 (the low byte of each word), modelled from the game's traffic; the
Z80 and its BIOS are not run. At reset the settings EEPROM is in bytes
0x100-0x17f (the 93C46's words byte-swapped from MAME's image). Command 1
at byte 0x20 latches the ADC (bytes 0-7: wheel, accelerator, brake) and
the switches (8-14: IN.0, IN.1); 2 and 3 store and load the EEPROM. Byte
0x11 is the drive board's command. Ours completes a command at once, so
`m1native` counts the polls MAME's slower board needed apart from real
differences.

A gameplay run scripts the controls with `scripts/m1_inputs.lua` (a MAME
`-autoboot_script`); `m1native --inputs` sets the same controls at each
vblank:

    M1INPUT_SCRIPT=scripts/inputs/vr_game.txt M1TRACE_LOG=vr_game.m1tr \
        M1TRACE_FRAMES=vr_game_frames.txt M1TRACE_PCS=vr_pcs_game.txt \
        extern/mame/m1 vr ... -seconds_to_run 122 -autoboot_script scripts/m1_inputs.lua
    m1native ROMS vr_game.m1tr --frames vr_game_frames.txt --inputs scripts/inputs/vr_game.txt

`M1INPUT_SNAPS=264,515` also saves those frames' screens as PNGs (in
`-snapshot_directory`), to set beside `m1run --dump`.

A read can differ where a control changes: MAME applies a frame's
controls at its frame callback, not at the vblank ours uses.

### EPR-14869B (Virtua Fighter)

Virtua Fighter's board runs the B BIOS (MAME's vf machine; the others run
EPR-14869), whose timing the game sees: `RomSet::io_bios_b` switches
`IoBoard` to a timed model of its Z80 loop, its events at the times
MAME's Z80 makes them (constants in `io_board.cpp`, measured on MAME). The
loop at 004D writes byte 0x21 (the EEPROM words written so far, 0x40 when
done), then reads the command at 0x20: 1 latches the ADC, IN.0, IN.1 and
IN.2 (player 2) into bytes 0-14 and clears 0x20 (about 7 ms), 2 copies
0x100-0x17f aside and sets 0x21 to 0, after which each idle pass writes
one EEPROM word (2,619 + 1.75 us for each bit set), 3 clears 0x20 and
writes the copy back. Each byte lands on the dual-port RAM at its own
time, so the V60's polls see what MAME's do.

### model1io2 (Wing War)

Wing War's board is the advanced 837-10859 (MAME's `model1io2`, BIOS
EPR-16891 from the device set `model1io2.zip`). Here the Z80 does run: its
BIOS is recompiled by `dsbrecomp --io2` into `gen/<set>_io/io_gen.cpp`
(`io2_none.cpp` stands in for sets without the board) and drives
`IoBoard2` (`src/model1/io_board2.{h,cpp}`), MAME's 315-5338A, MSM6253,
93C46 and CTC as plain members. The V60 sees the MB8421 dual-port RAM at
C00000. Before each V60 access the Z80 runs up to the start of the V60's
MAME timeslice (`Copro::slice_start_atto`); the board's CTC ticks, EEPROM
write cycle and watchdog end slices (`IoBoard2::latest_cut`, handed to the
copro by `set_cuts`). `m1run --io-log FILE` writes the board's accesses
with Z80 cycles, to diff with MAME's.

## Sound

The sound board is the one Daytona has (`src/runtime/sound_board.cpp`:
68000, YM3438, two MultiPCMs), given Virtua Racing's program and its rate
(1,041,300 instructions a second under MAME; each set has its own, `RomSet::sound_ips`: swa 762,760). The V60 only writes the
UART at C40000; VR takes no sound interrupt. `m2sndrecomp` recompiles the
program with `seeds/vr_snd.txt`, and `m1sndcheck` holds it to MAME's sound
CPU log (patch 0003's `M2TRACE_SNDLOG`):

    m1rom ROMS m1audio vr_snd.bin
    m2sndrecomp vr_snd.bin gen/vr_snd/snd_gen.cpp --seeds seeds/vr_snd.txt
    M2TRACE_SNDLOG=vr_snd.log ... extern/mame/m1 vr ... -wavwrite vr_mame.wav
    m1sndcheck vr_snd.bin vr_snd.log
    m1native ROMS vr_game.m1tr --frames ... --inputs ... --sound vr_ours.wav

The board runs on instruction counts, not cycles, so its time drifts from
MAME's by a few tens of milliseconds.

### Digital Sound Board (Star Wars Arcade)

swa's music and speech come from Sega's Digital Sound Board
(`src/model1/dsb.{h,cpp}`, after MAME's `dsbz80.cpp`): a Z80 that listens
to the sound 68000's UART (the bytes it transmits, `SoundBoard::take_tx`,
ten bits each at 31,250 baud) and plays MPEG layer II streams from the
board's 8 MB of ROM through MAME's decoder (`src/model1/mpeg_audio.cpp`) at
32 kHz stereo. The Z80 program is about 1 KB; `tools/dsbrecomp` recompiles
all of it (391 instructions, T-states counted at 4 MHz) into
`gen/swa_dsb/dsb_gen.cpp`. As in MAME, the decoder runs only when the stream
updates, 50 times a second of board time, and sees the registers as they
are then. `dsbcheck` holds the board to MAME's (`scripts/dsb_tap.lua` logs
the bytes the 68000 sent, the Z80's reads and its register writes):

    DSB_LOG=mame_dsb.log M1INPUT_SCRIPT=... extern/mame/m1 swa ... -autoboot_script scripts/dsb_tap.lua
    dsbcheck ROMS mame_dsb.log [--wav dsb.wav]

120 s of attract and play: all 10,669 reads and writes are MAME's, within
45 us. The core gives the board's output as audio stream 2.

## The core

`src/model1/board.cpp` runs the board on its own, a frame at a time, on
MAME's clock: the V60 at 8 cycles an instruction (MAME's average) plus
the cycles it waits, in 80 MHz ticks. A frame is 278,144 V60 cycles (656 x
424 pixels at 16 MHz): the vblank interrupt (level 1, vector 0x41) at line
384, the screen at line 0, 40 lines later. `src/core/m1core.cpp` puts it
behind `include/afcore.h` as `afcore_vr` (CMake `M1_CORE`):

    scripts/recompile.py --set vr --rom vr.7z --core   # libafcore_vr / afcore_vr.framework
    scripts/recompile.py --set vr --rom vr.7z          # m1run and the check tools
    m1run ROMS FRAMES [--inputs SCRIPT] [--frames MAME_FRAMES] [--dump DIR --every N] [--wav FILE]

The code comes from `seeds/vr.txt` and `seeds/vr_snd.txt`, no MAME needed.

What makes the time MAME's:

- The TGP (`m1::Copro`, timed): 40 MHz / 3, which MAME rounds up to
  13,333,334 Hz (74,999,996,250 attoseconds a cycle, not quite 6 ticks), a
  cycle an instruction and two for the ALU ops MAME's mb86233 charges two.
  Its time is its cycle count (MAME adds whole cycles, halted or not);
  `tgp_end` counts the cycles that fit by a time the way MAME's scheduler
  divides. In each of MAME's timeslices (a microsecond, its UART clock's
  timer, or up to a timer's tick) the V60 runs first, so it sees the
  output words of the TGP instructions that started a cycle or more
  before the slice's start (each word carries the TGP cycle that made it);
  a read from an empty FIFO syncs the TGP to the V60 and costs a retry,
  and if it is still empty halts the V60 after that instruction until
  the TGP pushes. Releasing the halt aborts the TGP's slice after its
  push instruction; the sound 68000 (10 MHz, listed before the halted
  V60) runs up to there in whole cycles and the V60 eats whole cycles up
  to the sound CPU (`v60_resume`). A write to a full input FIFO the TGP
  has made room for by then costs nothing; a TGP waiting for input starts
  again at the V60's push.
- The timers (E00008/A, level 0): swa's UART queue pump, Virtua
  Fighter's 128 us tick. A period write starts the timer at the write's
  end (MAME's time of a V60 access is its instruction's end), and each
  tick ends a slice (`Copro::timer_tick`).
- Interrupts: MAME checks for one after every V60 instruction, so the
  generated code returns as soon as one is pending and enabled (after
  UPDPSW, LDPR, RETIU, RETIS, and between table entries).
- The I/O board's wait state (`dpram_r`: a cycle for each byte-lane read)
  and its BIOS start-up: under MAME the game's first command completes
  2,507,400 cycles after reset, about nine frames in.

With them, `m1run` free-running (no trace) matches MAME's screens: 30 s
of attract, 1,725 of 1,725 identical; the scripted race with its controls,
7,000 of 7,000.

The code comes from MAME's runs (`seeds/vr.txt`: attract, races on all
three courses, manual shifting, game over and name entry, every test
menu, two linked cabinets) plus `m1recomp --speculate` (what follows a
return or jump, and what ROM words point at: routines no run took, such
as one a linked race reached). Each scripted run (`m1run --inputs`
against MAME's frames) matches MAME's screens. The scripts set MAME's
fields at its frame callback: an analogue field reads the new value at
once, a switch takes it at the next frame's input update, so `m1run`
applies switches a frame after the script's frame.

Controls: the wheel 0x01-0xff, the pedals 0x01-0xff, `AFCORE_SHIFT_UP` /
`AFCORE_SHIFT_DOWN`, or a change of `afcore_input.gear` held for 4
frames as a shift. Saves: `ioboard_eeprom.bin` (the settings) and
`backup_ram.bin` (400000, 64 KiB).

## Link play

`src/model1/comm_board.{h,cpp}` is MAME's m1comm in its simulation mode
(BSD-3, THIRD_PARTY.md): shared RAM at B00000-B00FFF, CN at B01000, FG at
B01002, ticked at line 384 before the vblank interrupt. Its protocol is
not Model 2's: byte 1 is the cabinet's role (1 master, 2 slave, 0 relay),
the master numbers the ring (0xff, then the count in 0xfe) 232 frames
after both cables are up, and every frame each cabinet with its ready
flag (byte 4) sends its 0x1c4-byte block from byte 0x10, kept by every
cabinet at 0x10 + id * 0x1c4; the master adds ten status bytes (0xfd)
and a vsync (0xfc). The bytes go over `app::TcpLink` (the core's
`link_start`, as for Model 2). With no link the board is still there and
reports "waiting" (byte 0 = 5) once the game enables it, as MAME with no
other cabinet does.

Each cabinet's role is the test menu's GAME SYSTEM, LINK ID (NO LINK,
LIVE, SLAVE, MASTER; red moves, blue changes, then EXIT, YES to save, and
EXIT from the main menu); it is saved in the I/O board's EEPROM (byte 12:
1 master, 2 slave). With MASTER or SLAVE the game shows NETWORK CHECKING
until the link is up.

Two cabinets:

    m1run ROMS FRAMES --nvram MASTER_DIR --link-listen 15201 --link-next 127.0.0.1:15202 [--link-sync]
    m1run ROMS FRAMES --nvram SLAVE_DIR  --link-listen 15202 --link-next 127.0.0.1:15201 [--link-sync]

(`--save-nvram DIR` keeps a run's EEPROM and backup RAM; `--link-sync` is
MAME's `-comm_framesync`.) From the same NVRAM and controls as two linked
MAMEs (`-comm_localport`/`-comm_remoteport`, `-comm_framesync`), our two
cabinets show MAME's screens up to the frame the link comes up, then race
each other as MAME's do. After that no two runs agree screen for screen,
MAME's own included: the link's timing is the host's.

## Tools

| Tool | Checks |
| --- | --- |
| `m1replay ROMS TRACE` | The interpreter against the trace: every device access, interrupt and sample. |
| `m1native ROMS TRACE [--check]` | The recompiled code against the trace. `--check` also steps the interpreter beside it and stops at the first register difference. |
| `m1recomp ROMS OUTDIR --seeds seeds/vr.txt [--pcs FILE...] [--reduce OUT] [--speculate]` | Generates the native code; `--reduce` keeps the fewest seeds that reach the same code (how `seeds/vr.txt` was made from MAME's harvests); `--speculate` adds the likely code no seed reaches (`recompile.py` passes it). Re-run cmake afterwards (the chunks are globbed). |
| `m1run ROMS FRAMES [...]` | The board free-running (the core's code); `--frames` measures its screens against MAME's; `--nvram`/`--save-nvram` load and keep the settings; `--link-listen`/`--link-next` link it to another. |
| `m1rom ROMS REGION OUT [--set swa]` | One de-interleaved region of the set (for `m2tgprecomp --model1`, `m2sndrecomp`, `dsbrecomp`). |
| `dsbrecomp DSB_CPU.bin OUT.cpp` | The Digital Sound Board's Z80 program as C++. |
| `dsbcheck ROMS MAME_DSB.log [--wav OUT]` | The Digital Sound Board against MAME's (`scripts/dsb_tap.lua`). |
| `m1traceinfo TRACE [--from A --to B] [--dump N]` | Device accesses in a trace, grouped, or listed. |
