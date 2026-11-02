// MAME's 315-5881 decryption (mame/crypt.inc) as rt::Crypt5881's members.
#include "runtime/crypt5881/crypt5881.h"

#include <cassert>

namespace rt {

namespace {
// MAME's bitswap<B>(val, b...): the listed bits of val, the first the highest.
template <unsigned B, typename T, typename... U> constexpr T bitswap(T val, U... b) {
    static_assert(sizeof...(b) == B);
    T r = 0;
    ((r = T(r << 1 | ((val >> b) & 1))), ...);
    return r;
}
} // namespace

#include "runtime/crypt5881/mame/crypt.inc"

} // namespace rt
