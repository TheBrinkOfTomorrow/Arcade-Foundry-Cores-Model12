// Support for a Model 1 sound board's recompiled 68000 beside a Model 2A
// board's SCSP 68000 (tools/m2sndrecomp --m1snd output: Manx TT's engine
// sounds): the sound 68000's helpers (snd_gen_support.h) under their own names.
#pragma once

#include "runtime/snd_gen_support.h"

#include <cstdint>

namespace m1sndgen {

using Env = sndgen::Env;

bool has_code(uint32_t addr);   // generated
void run(Env &e);               // generated: runs from c.pc until it leaves recompiled code or the schedule ends
uint64_t native_instructions(); // generated: instructions recompiled (all of them)

} // namespace m1sndgen
