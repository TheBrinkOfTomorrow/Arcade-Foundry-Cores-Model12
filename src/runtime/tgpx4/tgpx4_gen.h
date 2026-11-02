// The recompiled TGPx4 program (tools/m2tgpx4recomp writes its definition
// under build/, from the game's ROM).
#pragma once

#include "runtime/tgpx4/tgpx4.h"

namespace rt::tgpx4gen {

extern const uint32_t upload_words, upload_crc32; // the i960's upload (32-bit halves of the 64-bit instructions)
extern const uint32_t program_size;               // instructions recompiled, from address 0

enum class Stop {
    Wait,   // stalled on the empty input FIFO: run again once it has a word
    Budget, // ran `budget` instructions
};
// Runs from MAME execute_run's next address.
Stop run(rt::tgpx4::Tgpx4 &t, uint64_t budget);

} // namespace rt::tgpx4gen
