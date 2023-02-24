//
//  BRStellarAccountUtils.h
//  WalletKitCore
//
//  Created by Carl Cherry on 5/21/2019.
//  Copyright © 2019 Breadwinner AG. All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.
//
#ifndef BRStellar_account_utils_h
#define BRStellar_account_utils_h

#include "support/BRKey.h"
#include "support/BRInt.h"
#include "BRStellarBase.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generate the secret needed for public/private key pair
 *
 * @param paperKey  BIP39 paper key words
 *
 * @return key      BRKey object with the secret. In the case of Stellar the secret
 *                  is 32 bytes of random data that will be used to create a ed25519
 *                  key pair.
 */
BRKey createStellarKeyFromPaperKey(const char* paperKey);
BRKey createStellarKeyFromSeed(UInt512 seed);

BRStellarAccountID createStellarAccountIDFromStellarAddress(const char *stellarAddress);

BRKey deriveStellarKeyFromSeed (UInt512 seed, uint32_t index);


#ifdef __cplusplus
}
#endif

#endif // BRStellar_account_utils_h
