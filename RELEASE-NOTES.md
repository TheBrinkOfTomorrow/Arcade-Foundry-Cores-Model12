# Release notes

Newest first. Each release is also on the repository's Releases page.

## 1.2.0 (2026-11-06)

**Arcade Foundry Cores: Model 1 and 2 1.2.0**

The Model 1 and Model 2 cores as shipped with Insert Coin 1.2.0, which carries
the 1.1.0 and 1.2.0 work together (there was no separate 1.1.0 release). The
games are the same 4 Model 1 games and 32 Model 2 sets as in 1.0.0.

- Unseeded code: when a game reaches i960 code the recompiler didn't see, the
  core now runs it in the interpreter instead of stopping (`AF_DEBUG_M2_STRICT=1`
  keeps the hard stop for checks against MAME).
- ROM check: `check_roms` checks a ROM set with the same loaders the game
  uses, without starting it, so the app's Downloads can say whether a set is
  complete and right.
- Core audio goes through a ring buffer instead of a queue that was trimmed
  every frame, and each cabinet's inputs come from one table.
- Build: the Python the build tools need comes from one shared environment
  per machine, `~/venvs/afic` (Arcade-Foundry-Cores-Common's
  `tools/afic-venv/setup.sh`), instead of the system Python.
- Debug switches: the developer environment variables are now
  `AF_DEBUG_` plus their old name (`AF_DEBUG_M2_STRICT`); the old
  names still work in this release. The list is in Arcade-Foundry-Cores-Common's
  docs/DEBUG-SWITCHES.md.
- Build: the core interface header and the zip and 7z ROM set readers now come from the shared Arcade-Foundry-Cores-Common repo (the `common/` submodule). Clone with `--recursive`, or run `git submodule update --init`.
- The PS Vita, Dreamcast and Android/iOS (SDL) ports of the standalone build are archived on the `ports-archive` branch and removed from the main branches; the Arcade Foundry: Insert Coin app covers iOS. The Model 2 fast paths they contributed (`M2_FAST_PATHS`) stay.
- Model 2: the exact fast paths (`M2_FAST_PATHS`: rasterizer and 2D-layer caches, runtime fast paths, the 3D drawn on several threads) are now on for every platform, macOS and x86 Linux included, where they were on only for iOS, tvOS and arm64 Linux. The same pixels and sound, faster; `-DM2_FAST_PATHS=OFF` builds the reference.
- `M2_APP` (the windowed desktop app) defaults to OFF; `./setup.sh` and `scripts/recompile.py` turn it on, so the standalone build works as before. A plain `cmake` configure now builds the cores and tools only.

## 1.0.0 (2026-11-02)

**Arcade Foundry Cores: Model 1 and 2 1.0.0**

First release of the Model 1 and Model 2 cores for Arcade Foundry: Insert Coin, as shipped with Insert Coin 1.0.0.

- **Games:** 4 Model 1 games and 32 Model 2 sets, from Daytona USA and Sega Rally to Virtua Cop, Virtua Fighter 2, The House of the Dead and Dead or Alive. The full list is in the app's [GAMES.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Insert-Coin/blob/main/GAMES.md).
- **How it works:** each game's i960 program and the TGP program it uploads are statically recompiled to portable C++, and the hardware (geometrizer, rasterizer, tilemaps, sound board) is native C++. No interpreter and no emulation core. MAME is only a test oracle.
- **Platforms:** the main build runs on macOS and Linux.
- **Building:** `./setup.sh`. Start with `docs/getting-started.md`.

No ROM data is in this repository. You supply your own sets. This project started from Ben Templeman's daytona-arcade-recomp, whose history it keeps. BSD-3-Clause; third-party code is in `THIRD_PARTY.md`.

The release number is a snapshot label for "as shipped with Insert Coin 1.0.0"; each game core has its own version.
