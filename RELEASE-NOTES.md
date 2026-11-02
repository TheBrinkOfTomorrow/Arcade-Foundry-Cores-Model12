# Release notes

Newest first. Each release is also on the repository's Releases page.

## 1.0.0 (2026-11-02)

**Arcade Foundry Cores: Model 1 and 2 1.0.0**

First release of the Model 1 and Model 2 cores for Arcade Foundry: Insert Coin, as shipped with Insert Coin 1.0.0.

- **Games:** 4 Model 1 games and 32 Model 2 sets, from Daytona USA and Sega Rally to Virtua Cop, Virtua Fighter 2, The House of the Dead and Dead or Alive. The full list is in the app's [GAMES.md](https://github.com/TheBrinkOfTomorrow/Arcade-Foundry-Insert-Coin/blob/main/GAMES.md).
- **How it works:** each game's i960 program and the TGP program it uploads are statically recompiled to portable C++, and the hardware (geometrizer, rasterizer, tilemaps, sound board) is native C++. No interpreter and no emulation core. MAME is only a test oracle.
- **Platforms:** the main build runs on macOS and Linux.
- **Building:** `./setup.sh`. Start with `docs/getting-started.md`.

No ROM data is in this repository. You supply your own sets. This project started from Ben Templeman's daytona-arcade-recomp, whose history it keeps. BSD-3-Clause; third-party code is in `THIRD_PARTY.md`.

The release number is a snapshot label for "as shipped with Insert Coin 1.0.0"; each game core has its own version.
