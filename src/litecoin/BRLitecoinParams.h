//
//  BRLitecoinParams.h
//
//  Created by Aaron Voisine on 5/18/21.
//  Copyright (c) 2021 breadwallet LLC
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#ifndef BRLitecoinParams_h
#define BRLitecoinParams_h

#include "bitcoin/BRBitcoinChainParams.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LTC_BIP32_DEPTH 3
#define LTC_BIP32_CHILD ((const uint32_t []){ 44 | BIP32_HARD, 2 | BIP32_HARD, 0 | BIP32_HARD })

extern const BRBitcoinChainParams *ltcChainParams(bool mainnet);

static inline int ltcChainParamsHasParams (const BRBitcoinChainParams *params) {
    return ltcChainParams(true) == params || ltcChainParams(false) == params;
}


#ifdef __cplusplus
}
#endif

#endif // BRLitecoinParams_h
