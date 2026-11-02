// The Digital Sound Board's Z80 code for a set without the board (dsb.h):
// the board is never built for such a set, so this never runs.
#include "model1/dsb_z80.h"

#include <stdexcept>

namespace dsb {

void run(Z80 &, uint64_t) { throw std::runtime_error("DSB: no Z80 code generated for this set"); }

} // namespace dsb
