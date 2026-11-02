// license:BSD-3-Clause
// copyright-holders:Angelo Salese, ElSemi, Matthew Daniels
//
// The TGPx4's semantics, MAME's (mame/ops.inc), for the recompiled program
// only: ops.inc defines the status flags as short macros (AD, MD, F0...),
// so nothing else includes this.
#pragma once

#include "runtime/tgpx4/tgpx4.h"

namespace rt::tgpx4 {

#include "runtime/tgpx4/mame/ops.inc"

} // namespace rt::tgpx4
