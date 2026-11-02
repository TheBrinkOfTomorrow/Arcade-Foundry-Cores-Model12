#!/usr/bin/env python3
"""Write a scripted M2IN input stream (docs/trace-format.md).

  make_input.py HEADER.m2in SCRIPT OUT.m2in

HEADER is any input stream recorded on the same machine
(M2TRACE_RECORD_INPUT); only its field layout is used, so the scripted stream
matches the machine's fields exactly. SCRIPT is a text file:

  frames N                  total frames to write
  # comment
  <from>-<to> <name>=<v>    hold a control over a frame range (inclusive)
  <at> <name>=<v>           one frame only

Names: coin, service, test, start, vr1..vr4 (1 = pressed), gear0..gear4
(1 = selected), steer, accel, brake (raw 0x20-0xe0 ADC values). Manx TT:
start, shiftup, shiftdown (1 = pressed), throttle, brake, bank (raw 0x00-0xff). Virtua
Cop 2: start, start2, trigger1, trigger2 (1 = pressed), p1x, p1y, p2x, p2y (raw gun
positions in MAME's ranges, e.g. p1x 137-630, p1y 36-425; Gunblade NY: the same names,
its turrets' 8-bit values). Virtua Fighter 2:
start, start2, punch, kick, guard, up, down, left, right, and punch2 .. right2
for player 2 (1 = pressed). INDY 500: start, shiftup, shiftdown, view1, view2
(1 = pressed), steer, accel, brake (raw 0x00-0xff). Unscripted
controls rest at their recorded defaults. Later lines override earlier ones.
"""

import struct
import sys

# name -> (port, mask); digital unless listed in ANALOG.
CONTROLS = {
    "coin": (":IN0", 0x01), "service": (":IN0", 0x08), "test": (":IN0", 0x04),
    "start": (":IN0", 0x10),
    "vr1": (":IN0", 0x20), "vr2": (":IN0", 0x40), "vr3": (":IN0", 0x80), "vr4": (":IN1", 0x01),
    "gear0": (":GEARS", 0x01), "gear1": (":GEARS", 0x02), "gear2": (":GEARS", 0x04),
    "gear3": (":GEARS", 0x08), "gear4": (":GEARS", 0x10),
    "steer": (":STEER", 0xFF), "accel": (":ACCEL", 0xFF), "brake": (":BRAKE", 0xFF),
}
ANALOG = {"steer", "accel", "brake"}
# Sega Rally (Model 2A-CRX) moves start to 0x40 (rt::kIn0Start).
MODEL2A_START = (":IN0", 0x40)
# Manx TT (manxtt, manxttc): start at 0x40, two shift buttons on IN1, and
# throttle, brake and bank (lean) as analogue inputs. Motor Raid (motoraid)
# the same, with punch and kick for the shift buttons.
MANX = {
    "start": MODEL2A_START, "shiftup": (":IN1", 0x10), "shiftdown": (":IN1", 0x20),
    "throttle": (":THROTTLE", 0xFF), "brake": (":BRAKE", 0xFF), "bank": (":BANK", 0xFF),
}

# INDY 500 (and the other games on its ports: Super GT 24h, Over Rev, Touring
# Car): start at 0x40, the shift buttons and the two view buttons on IN1.
INDY500 = {
    "start": MODEL2A_START, "shiftup": (":IN1", 0x10), "shiftdown": (":IN1", 0x20),
    "view1": (":IN1", 0x01), "view2": (":IN1", 0x02),
}

# Virtua Cop 2: MAME's model2 IN0 (start 0x10, start 2 0x20), triggers on IN1,
# and the guns' raw positions.
VCOP2 = {
    "start2": (":IN0", 0x20), "trigger1": (":IN1", 0x01), "trigger2": (":IN1", 0x02),
    "p1x": (":P1_X", 0x3FF), "p1y": (":P1_Y", 0x3FF), "p2x": (":P2_X", 0x3FF), "p2y": (":P2_Y", 0x3FF),
}

# Virtua Fighter 2: MAME's model2 IN0 (start 2 0x20); punch, kick and guard on
# bits 0-2 and the joystick on bits 4-7 of IN1 (player 1) and IN2 (player 2).
VF2 = {"start2": (":IN0", 0x20)}
for _port, _sfx in ((":IN1", ""), (":IN2", "2")):
    for _name, _mask in (("punch", 0x01), ("kick", 0x02), ("guard", 0x04),
                         ("down", 0x10), ("up", 0x20), ("right", 0x40), ("left", 0x80)):
        VF2[_name + _sfx] = (_port, _mask)


def read_header(blob):
    assert blob[:4] == b"M2IN" and struct.unpack_from("<I", blob, 4)[0] == 1, "not an M2IN v1 stream"
    p = 8
    n = struct.unpack_from("<H", blob, p)[0]
    game = blob[p + 2:p + 2 + n].decode()
    p += 2 + n
    nf = struct.unpack_from("<H", blob, p)[0]
    p += 2
    fields = []
    for _ in range(nf):
        ln = struct.unpack_from("<H", blob, p)[0]
        port = blob[p + 2:p + 2 + ln].decode()
        p += 2 + ln
        mask, defv, analog = struct.unpack_from("<IIB", blob, p)
        p += 9
        fields.append((port, mask, defv, bool(analog)))
    return blob[:p], game, fields


def main():
    header_path, script_path, out_path = sys.argv[1:4]
    header, game, fields = read_header(open(header_path, "rb").read())
    controls = dict(CONTROLS)
    if game.startswith("srallyc"):
        controls["start"] = MODEL2A_START
    if game.startswith("manxtt"):
        controls.update(MANX)
    if game == "motoraid":
        controls.update(MANX)
        controls.update({"punch": (":IN1", 0x10), "kick": (":IN1", 0x20)})
    if game == "desert":  # VR1-3 are vr1-vr3; the shift lever (a toggle in MAME), machine gun, cannon on IN1
        controls.update({"shift": (":IN1", 0x01), "gun": (":IN1", 0x10), "cannon": (":IN1", 0x20)})
    if game == "skytargt":  # the flight stick (8-bit ADC values, 0x80 centred), machine gun, missile, view change
        controls.update({"start": MODEL2A_START, "view": (":IN0", 0x20), "gun": (":IN1", 0x10), "missile": (":IN1", 0x20),
                         "stickx": (":STICKX", 0xFF), "sticky": (":STICKY", 0xFF)})
    if game in ("vcop2", "vcop", "hotdo", "hotd"):
        controls.update(VCOP2)
    if game == "bel":  # Gunblade NY's turrets, test and service swapped, the missile buttons
        controls.update({k: (port, 0xFF) if k[0] == "p" else (port, mask) for k, (port, mask) in VCOP2.items()})
        controls.update({"test": (":IN0", 0x08), "service": (":IN0", 0x04), "missile": (":IN1", 0x10), "missile2": (":IN1", 0x20)})
    if game == "rchase2":  # the turrets: 8-bit ADC values before MAME's reversal (p1x 0x3a-0xca, p2x 0x34-0xc7, y 0x1c-0xcb)
        controls.update({k: (port, 0xFF) if k[0] == "p" else (port, mask) for k, (port, mask) in VCOP2.items()})
    if game == "gunblade":  # the turrets: 8-bit ADC values (p1x 0x69-0xff, p2x 0x00-0x96, y 0x11-0xae)
        controls.update({k: (port, 0xFF) if k[0] == "p" else (port, mask) for k, (port, mask) in VCOP2.items()})
    if game == "doab":  # MAME's doa ports: hold, punch and kick on bits 0-2, the joysticks as vf2's
        controls.update(VF2)
        for sfx, port in (("", ":IN1"), ("2", ":IN2")):
            controls.update({"hold" + sfx: (port, 0x01), "punch" + sfx: (port, 0x02), "kick" + sfx: (port, 0x04)})
            controls.pop("guard" + sfx, None)
    if game in ("vf2", "fvipers", "von", "vstriker", "zerogun", "pltkids", "dynamcop", "lastbrnx", "schamp", "dynabb97"):  # Virtual-On: punch/kick = shot/dash, the left stick; ...2 the right; Virtua Striker: punch long pass, kick shoot, guard short pass; Zero Gunner, Pilot Kids: punch shot, kick special; Dynamite Cop: guard jump
        controls.update(VF2)
    if game == "topskatr":  # the curving and the slide (8-bit, 0x80 centred), select left and right, jump front and tail
        controls.update({"start": MODEL2A_START, "curving": (":CURVING", 0xFF), "slide": (":SLIDE", 0xFF), "selright": (":IN0", 0x10),
                         "selleft": (":IN0", 0x80), "jumpfront": (":IN0", 0x20), "jumptail": (":IN1", 0x01)})
    if game == "skisuprg":  # the swing and the inclining (8-bit, 0x80 centred), select 1-3, zoom in and out
        controls.update({"swing": (":SWING", 0xFF), "inclining": (":INCLINING", 0xFF), "sel1": (":IN0", 0x40), "sel2": (":IN0", 0x80),
                         "sel3": (":IN0", 0x10), "zoomin": (":IN0", 0x20), "zoomout": (":IN1", 0x01)})
    if game == "segawski":  # the slide (8-bit, 0x80 centred), set, select up and down, pitch left and right
        controls.update({"slide": (":SLIDE", 0xFF), "set": (":IN1", 0x01), "selup": (":IN1", 0x02), "pitchl": (":IN1", 0x04),
                         "pitchr": (":IN1", 0x08), "seldown": (":IN0", 0x40)})
    if game == "waverunr":  # the handle bar, roll, throttle lever and pitch (8-bit fields, 0x80 centred), view
        controls.update({"start": MODEL2A_START, "view1": (":IN1", 0x01), "handle": (":HANDLE", 0xFF), "roll": (":ROLL", 0xFF),
                         "throttle": (":THROTTLE", 0xFF), "pitch": (":PITCH", 0xFF)})
    if game == "dynabb97":  # the bat swings (8-bit ADC values, 0 at rest)
        controls.update({"bat": (":BAT1", 0xFF), "bat2": (":BAT2", 0xFF)})
    if game.startswith(("indy500", "stcc")) or game in ("sgt24h", "overrevb"):
        controls.update(INDY500)
    index = {(port, mask): i for i, (port, mask, _, _) in enumerate(fields)}

    total = 0
    events = []  # (from, to, field index, raw value)
    for lineno, line in enumerate(open(script_path), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if parts[0] == "frames":
            total = int(parts[1])
            continue
        rng, assign = parts
        lo, _, hi = rng.partition("-")
        lo, hi = int(lo), int(hi or lo)
        name, _, val = assign.partition("=")
        if name not in controls:
            sys.exit(f"{script_path}:{lineno}: unknown control {name}")
        port, mask = controls[name]
        fi = index[(port, mask)]
        _, _, defv, analog = fields[fi]
        v = int(val, 0)
        if analog:
            raw = v & mask
        else:  # digital: pressed flips the field away from its default
            raw = (defv ^ mask) if v else defv
        events.append((lo, hi, fi, raw))

    if total <= 0:
        sys.exit("script needs 'frames N'")
    frames = []
    for f in range(total):
        row = [defv for (_, _, defv, _) in fields]
        for lo, hi, fi, raw in events:
            if lo <= f <= hi:
                row[fi] = raw
        frames.append(struct.pack("<%dI" % len(row), *row))
    with open(out_path, "wb") as out:
        out.write(header)
        out.write(b"".join(frames))
    print(f"{out_path}: {total} frames, {len(fields)} fields, {len(events)} scripted ranges")


if __name__ == "__main__":
    main()
