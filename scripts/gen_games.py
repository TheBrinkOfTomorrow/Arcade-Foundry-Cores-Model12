#!/usr/bin/env python3
"""Write the files that list this repo's games from games.toml.

  gen_games.py          rewrite them
  gen_games.py --check  only compare (CI): exit 1 when one is out of date

Writes cmake/games.cmake (the CMake set lists, each set's board and Model 1
title), scripts/game_sets.py (the scripts' set lists) and the version lines in
src/core/core_versions.h. A new game: a [[game]] in games.toml, then this.
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "common", "tools", "games"))
import afgames  # noqa: E402

HEADER = "# Written by scripts/gen_games.py from games.toml: edit that, not this.\n"
# Model 2's boards, as CMake's M2_BOARD and MAME's driver classes name them
M2_BOARD = {"model2": "model2o", "model2a": "model2a", "model2b": "model2b", "model2c": "model2c"}


def generate(out):
    games = afgames.load_core(os.path.join(ROOT, "games.toml"))
    m1 = [g for g in games if g["board"] == "model1"]
    m2 = [g for g in games if g["board"] in M2_BOARD]
    if len(m1) + len(m2) != len(games):
        raise afgames.ManifestError("only Model 1 and Model 2 boards belong in this repo")

    cm = [HEADER, "# Model 2 sets (M2_ROMSET) and the board each runs on (M2_BOARD)\n",
          f"set(M2_SETS {afgames.cmake_list(g['set'] for g in m2)})\n"]
    cm += [f"set(M2_BOARD_{g['set']} {M2_BOARD[g['board']]})\n" for g in m2]
    cm += ["# Model 1 sets (M1_ROMSET) and their titles (afcore_api.title)\n",
           f"set(M1_SETS {afgames.cmake_list(g['set'] for g in m1)})\n"]
    cm += [f"set(M1_TITLES_{g['set']} {afgames.c_string(g['title'])})\n" for g in m1]
    out.write("cmake/games.cmake", "".join(cm))

    def sets(board):
        return tuple(g["set"] for g in m2 if g["board"] == board)

    py = ['"""The set lists, written by scripts/gen_games.py from games.toml: edit that, not this."""', "",
          "# Model 2 sets, in games.toml's order (recompile.py --set, seed_scan.py --set)",
          f"M2_SETS = {tuple(g['set'] for g in m2)!r}",
          "# by board: the original Model 2, 2A-CRX, 2B-CRX (a SHARC for the TGP), 2C-CRX (a TGPx4)",
          f"MODEL2O_SETS = {sets('model2')!r}",
          f"MODEL2A_ONLY_SETS = {sets('model2a')!r}",
          f"MODEL2B_SETS = {sets('model2b')!r}",
          f"MODEL2C_SETS = {sets('model2c')!r}",
          "# Model 1 sets (cmake/model1.cmake): their own pipeline",
          f"MODEL1_SETS = {tuple(g['set'] for g in m1)!r}",
          "# Other names a set's archive may have (MAME's parent holding a clone's whole set)",
          f"OTHER_NAMES = {dict((g['set'], tuple(g['other_names'])) for g in games if g['other_names'])!r}",
          ""]
    out.write("scripts/game_sets.py", "\n".join(py))

    out.region("src/core/core_versions.h", "versions",
               [f"#define AFCORE_CORE_VERSION_{g['set']} AFCORE_MAKE_VERSION({', '.join(map(str, afgames.version_tuple(g)))})"
                for g in games])


if __name__ == "__main__":
    afgames.main(generate, ROOT, __doc__)
