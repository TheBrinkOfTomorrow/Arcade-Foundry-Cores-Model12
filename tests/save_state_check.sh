#!/bin/sh
# Save states give the same game (docs/design/MODEL2.md, "Save states"): a
# run that saves a state after frame N, a run from that state and a run with
# no state at all must draw the same screen and play the same sound on every
# frame after N. All three play the same inputs (m2coreshot --play).
#
#   tests/save_state_check.sh BUILD_DIR ROMSET.zip N FRAMES
#
# BUILD_DIR has m2coreshot or m1coreshot (the set it was built for,
# M2_CORE=ON or M1_CORE=ON). Prints the
# first frame that differs, or "same". KEEP=1 keeps the work folder.
set -eu
build=$1 rom=$2 at=$3 frames=$4
tool=$build/m2coreshot
[ -x "$tool" ] || tool=$build/m1coreshot
work=$(mktemp -d)
if [ -n "${KEEP:-}" ]; then echo "work: $work"; else trap 'rm -rf "$work"' EXIT; fi
run() { # name, frames, extra args...
  name=$1
  shift
  "$tool" "$rom" "$@" --play --hashes "$work/$name.txt" >"$work/$name.log" 2>&1 ||
    { cat "$work/$name.log" >&2; exit 1; }
}
run plain "$frames"
run saved "$frames" --save-state-at "$at" --state "$work/state.bin"
run loaded $((frames - at)) --load-state "$work/state.bin"
grep -h 'saved\|loaded' "$work/saved.log" "$work/loaded.log"
status=0
awk -v at="$at" '$1 > at' "$work/plain.txt" >"$work/plain.after"
for other in saved loaded; do
  awk -v at="$at" '$1 > at' "$work/$other.txt" >"$work/$other.after"
  if cmp -s "$work/plain.after" "$work/$other.after"; then
    echo "$other: same ($(wc -l <"$work/$other.after" | tr -d ' ') frames after $at, screen and sound)"
  else
    first=$(diff "$work/plain.after" "$work/$other.after" | grep -m1 '^[<>]' | awk '{print $2}')
    echo "$other: differs from frame $first"
    status=1
  fi
done
exit $status
