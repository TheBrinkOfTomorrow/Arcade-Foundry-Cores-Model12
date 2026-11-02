// Support for the Digital Sound Board 2's recompiled 68000 (tools/m2sndrecomp
// --dsb2 output): the sound 68000's helpers (snd_gen_support.h) on a
// Cpu68k with its dsb2 map, under its own names.
#pragma once

#include "runtime/snd_gen_support.h"

#include <cstdint>

namespace dsb2gen {

using Env = sndgen::Env;

bool has_code(uint32_t addr);   // generated
void run(Env &e);               // generated: runs from c.pc until it leaves recompiled code or the schedule ends
uint64_t native_instructions(); // generated: instructions recompiled (all of them)

} // namespace dsb2gen
