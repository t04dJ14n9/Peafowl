#include "cryptoTools/Common/Defines.h"
#include "cryptoTools/Common/Log.h"
#include "cryptoTools/Crypto/PRNG.h"
#include "op.cpp"

using namespace osuCrypto;

using uint128_t = __uint128_t;

int main()
{
    // 初始化一个伪随机数生成器
    PRNG prng(ZeroBlock);

    // 生成一个随机的 osuCrypto::block 对象
    block b = prng.get<block>();

    // 使用 osuCrypto::block::u128() 方法将其转化为 uint128_t 对象
    uint128_t u = blockToUInt128(b);

    return 0;
}