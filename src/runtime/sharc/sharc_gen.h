// The recompiled SHARC program (tools/m2sharcrecomp writes its definition
// under build/, from the game's ROM).
#pragma once

#include "runtime/sharc/sharc.h"

namespace rt::sharcgen {

extern const uint32_t upload_words, upload_crc32; // the i960's boot upload (16-bit words)
extern const uint32_t program_base, program_size; // what it recompiled: program_size instructions at program_base

enum class Stop {
    Wait,   // at a wait on a FLAG input (a FIFO status line): run again once it changes
    Idle,   // IDLE
    Budget, // ran `budget` instructions
};
// Runs from the pipeline's next address (daddr).
Stop run(rt::sharc::Sharc &s, uint64_t budget);

} // namespace rt::sharcgen
