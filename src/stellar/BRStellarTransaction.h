//
//  BRStellarTransaction.h
//  WalletKitCore
//
//  Created by Carl Cherry on 5/21/2019
//  Copyright © 2019 Breadwinner AG. All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.
//
#ifndef BRStellar_transaction_h
#define BRStellar_transaction_h

#include "BRStellarBase.h"
#include "BRStellarAddress.h"
#include "BRStellarFeeBasis.h"
#include "support/BRKey.h"
#include "support/BRArray.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BRStellarTransactionRecord *BRStellarTransaction;
typedef struct BRStellarSerializedTransactionRecord *BRStellarSerializedTransaction;

/**
 * Create a Stellar transaction
 *
 * @param  sourceAddress  Stellar address of owner account
 * @param  targetAddress  Stellar address of recieving account
 *
 * @return transaction    a stellar transaction
 */
extern BRStellarTransaction
stellarTransactionCreate(BRStellarAddress sourceAddress,
                         BRStellarAddress targetAddress,
                         BRStellarAmount amount, // For now assume XLM drops.
                         BRStellarFeeBasis feeBasis);


extern BRStellarTransaction /* caller must free - stellarTransferFree */
stellarTransactionCreateFull (BRStellarAddress sourceAddress,
                             BRStellarAddress targetAddress,
                             BRStellarAmount amount, // For now assume XRP drops.
                             BRStellarFeeBasis feeBasis,
                             BRStellarTransactionHash hash,
                             uint64_t timestamp,
                             uint64_t blockHeight,
                              int error);

/**
 * Create a Stellar transaction
 *
 * @param  bytes    serialized bytes from the server
 * @param  length   length of above bytes
 *
 * @return transaction    a stellar transaction
 */
extern BRStellarTransaction /* caller must free - stellarTransactionFree */
stellarTransactionCreateFromBytes(uint8_t *tx_bytes, size_t tx_length);

/**
 * Clean up any memory for this transaction
 *
 * @param transaction  BRStellarTransaction
 */
extern void stellarTransactionFree(BRStellarTransaction transaction);

/**
 * Get the size of a serialized transaction
 *
 * @param  s     serialized transaction
 * @return size
 */
extern size_t stellarGetSerializedSize(BRStellarSerializedTransaction s);

/**
 * Get the raw bytes of a serialized transaction
 *
 * @param  s     serialized transaction
 * @return bytes uint8_t
 */
extern uint8_t* /* DO NOT FREE - owned by the transaction object */
stellarGetSerializedBytes(BRStellarSerializedTransaction s);

/**
 * Get the hash of a stellar transaction
 *
 * @param  transaction   a valid stellar transaction
 * @return hash          a BRStellarTransactionHash object
 */
extern BRStellarTransactionHash stellarTransactionGetHash(BRStellarTransaction transaction);

extern int stellarTransactionHasSource (BRStellarTransaction transaction,
                                       BRStellarAddress source);
extern int stellarTransactionHasTarget (BRStellarTransaction transaction,
                                       BRStellarAddress target);

extern BRStellarAddress
stellarTransactionGetSource(BRStellarTransaction transaction);
extern BRStellarAddress
stellarTransactionGetTarget(BRStellarTransaction transaction);
extern BRStellarAmount
stellarTransactionGetAmount(BRStellarTransaction transaction);

extern BRStellarFee
stellarTransactionGetFee(BRStellarTransaction transaction);

extern uint8_t* stellarTransactionSerialize(BRStellarTransaction transaction, size_t * bufferSize);

extern void
stellarTransactionSetMemo(BRStellarTransaction transaction, BRStellarMemo * memo);

extern int stellarTransactionHasError(BRStellarTransaction transaction);

extern int stellarTransactionIsInBlock (BRStellarTransaction transaction);
extern uint64_t stellarTransactionGetBlockHeight (BRStellarTransaction transaction);

extern BRStellarFeeBasis
stellarTransactionGetFeeBasis (BRStellarTransaction transaction);

#ifdef __cplusplus
}
#endif

#endif // BRStellar_transaction_h
