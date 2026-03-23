#include <gmp.h>
#include "cryptoTools/Common/Defines.h"

void block2mpz(osuCrypto::block b, mpz_t r)
{
    mpz_import(r, 2, -1, sizeof(unsigned long int), 0, 0, &(b.as<uint64_t>()));
}

osuCrypto::block mpz2block(mpz_t a)
{
    std::array<uint64_t,2> tmp={0ul,0ul};
    size_t bits = mpz_sizeinbase(a, 2);
    if (bits > sizeof(tmp) * 8) {
        printf("Warning: a is too large to fit in tmp array\n");
    }
    mpz_export(tmp.data(), NULL, -1, sizeof(uint64_t), 0, 0, a); // 将mpz_t类型的a导出到tmp数组中
    // std::cout << tmp[0]<<" "<<tmp[1]<<std::endl;
    return osuCrypto::block(tmp[1], tmp[0]); // 使用tmp数组中的两个元素构造一个osuCrypto::block类型并返回
}