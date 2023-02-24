//
//  test.h
//  CoreTests
//
//  Created by Ed Gamble on 2/14/19.
//  Copyright © 2019 Breadwinner AG.  All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.

#ifndef BR_Ethereum_Test_H
#define BR_Ethereum_Test_H

#include "ethereum/blockchain/BREthereumNetwork.h"
#include "ethereum/blockchain/BREthereumAccount.h"
#include "WKSync.h"
#include "WKAccount.h"
#include "WKNetwork.h"
#include "bitcoin/BRBitcoinChainParams.h"

#ifdef __cplusplus
extern "C" {
#endif

// Support

// JSON
extern void runJsonTests (void);

// Util
extern void runUtilTests (void);

// RLP
extern void runRlpTests (void);


// Event
extern void runEventTests (void);

// Ethereum

// Base
extern void runBaseTests (void);

// Block Chain
extern void runBcTests (void);

// Transactions
extern void runTransactionTests (int reallySend);

// Contract
extern void runContractTests (void);

// Structure
extern void runStructureTests (void);

#if REFACTOR
// LES
extern void runLESTests(const char *paperKey);
extern void runNodeTests (void);

// EWM

extern void
runEWMTests (const char *paperKey,
             const char *storagePath);
    
extern void
runSyncTest (BREthereumNetwork network,
             BREthereumAccount account,
             WKSyncMode mode,
             BREthereumTimestamp accountTimestamp,
             unsigned int durationInSeconds,
             const char *storagePath);
//
extern void
installTokensForTest (void);

#endif

extern void
runPerfTestsCoder (int repeat, int many);

// Bitcoin
typedef enum {
    BITCOIN_CHAIN_BTC,
    BITCOIN_CHAIN_BCH,
    BITCOIN_CHAIN_BSV,
    BITCOIN_CHAIN_LTC,
    BITCOIN_CHAIN_DOGE
} BRBitcoinChain;

extern const BRBitcoinChainParams*
getChainParams (BRBitcoinChain chain, int isMainnet);

extern const char *
getChainName (BRBitcoinChain chain);

extern int BRRunSupTests (void);

extern int BRRunTests();

extern int BRRunTestsSync (const char *paperKey,
                           BRBitcoinChain bitcoinChain,
                           int isMainnet);

#if REFACTOR
extern int BRRunTestWalletManagerSync (const char *paperKey,
                                       const char *storagePath,
                                       BRBitcoinChain bitcoinChain,
                                       int isMainnet);

extern int BRRunTestWalletManagerSyncStress(const char *paperKey,
                                            const char *storagePath,
                                            uint32_t earliestKeyTime,
                                            uint64_t blockHeight,
                                            BRBitcoinChain bitcoinChain,
                                            int isMainnet);

extern int BRRunTestsBWM (const char *paperKey,
                          const char *storagePath,
                          BRBitcoinChain bitcoinChain,
                          int isMainnet);
#endif

extern void BRRandInit (void);

// testWalletKit.c
extern void runWalletKitTests (void);

// testWalletConnect.c
extern void runWalletConnectTests (void);

// testBTCWalletManager.c
extern void runBTCWalletManagerTests (void);

extern WKBoolean
runWalletKitTestsWithAccountAndNetwork (WKAccount account,
                                        WKNetwork network,
                                        const char *storagePath);

// Ripple
extern void
runRippleTest (void /* ... */);

// Hedera
extern void
runHederaTest (void);

// Tezos
extern void
runTezosTest (void);

// Stellar
extern void
runStellarTest (void);

// __NEW_BLOCKCHAIN_TEST_DEFN__

#ifdef __cplusplus
}
#endif

#endif /* BR_Ethereum_Test_H */
