#include "cryptoTools/Common/Defines.h"

using uint128_t = __uint128_t;


uint128_t blockToUInt128(osuCrypto::block b) {
    vec = &(b.as<uint64_t>())
    return (static_cast<uint128_t>(vec[1]) << 64) | vec[0];
}

