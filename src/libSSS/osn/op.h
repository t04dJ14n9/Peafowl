// 以下是为你生成的头文件，你可以将它保存为.h文件，并在你的.c文件中包含它
#ifndef BLOCK2MPZ_H // 防止头文件重复包含
#define BLOCK2MPZ_H

#include "libOTe/Base/BaseOT.h"

#include <gmp.h>
#include "cryptoTools/Common/Defines.h"

// 将osuCrypto::block类型转换为mpz_t类型的函数，参数b是要转换的block，参数r是转换后的mpz_t
void block2mpz(osuCrypto::block b, mpz_t r);

// 将mpz_t类型转换为osuCrypto::block类型的函数，参数a是要转换的mpz_t，返回值是转换后的block
osuCrypto::block mpz2block(mpz_t a);

#endif

