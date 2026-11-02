// The model1io2 board's Z80 code for a set without the board (io2_z80.h):
// the board is never built for such a set, so this never runs.
#include "model1/io2_z80.h"

#include <stdexcept>

namespace io2 {

void run(Z80 &, uint64_t) { throw std::runtime_error("model1io2: no Z80 code generated for this set"); }

} // namespace io2
