//
//  BRBitcoinTransaction.h
//
//  Created by Aaron Voisine on 8/31/15.
//  Copyright (c) 2015 breadwallet LLC
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

#ifndef BRTransaction_h
#define BRTransaction_h

#include "support/BRKey.h"
#include "support/BRInt.h"
#include "support/BRAddress.h"
#include <stddef.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TX_FEE_PER_KB        1000ULL     // standard tx fee per kb of tx size (bitcoind 0.12 default min-relay fee-rate)
#define TX_OUTPUT_SIZE       34          // estimated size for a typical transaction output
#define TX_INPUT_SIZE        148         // estimated size for a typical compact pubkey transaction input
#define TX_MIN_OUTPUT_AMOUNT (TX_FEE_PER_KB*3*(TX_OUTPUT_SIZE + TX_INPUT_SIZE)/1000) //no txout can be below this amount
#define TX_MAX_SIZE          100000      // no tx can be larger than this size in bytes
#define TX_UNCONFIRMED       INT32_MAX   // block height indicating transaction is unconfirmed
#define TX_MAX_LOCK_HEIGHT   500000000   // a lockTime below this value is a block height, otherwise a timestamp

#define TXIN_SEQUENCE        UINT32_MAX  // sequence number for a finalized tx input

#define SATOSHIS             100000000LL
#define MAX_MONEY            (21000000LL*SATOSHIS)

typedef struct {
    UInt256 txHash;
    uint32_t index;
    uint64_t amount;
    uint8_t *script;
    size_t scriptLen;
    uint8_t *signature;
    size_t sigLen;
    uint8_t *witness;
    size_t witLen;
    uint32_t sequence;
} BRBitcoinTxInput;

size_t btcTxInputAddress(const BRBitcoinTxInput *input, char *address, size_t addrLen, BRAddressParams params);
void btcTxInputSetAddress(BRBitcoinTxInput *input, BRAddressParams params, const char *address);
void btcTxInputSetScript(BRBitcoinTxInput *input, const uint8_t *script, size_t scriptLen);
void btcTxInputSetSignature(BRBitcoinTxInput *input, const uint8_t *signature, size_t sigLen);
void btcTxInputSetWitness(BRBitcoinTxInput *input, const uint8_t *witness, size_t witLen);

typedef struct {
    uint64_t amount;
    uint8_t *script;
    size_t scriptLen;
} BRBitcoinTxOutput;

#define BR_TX_OUTPUT_NONE ((const BRBitcoinTxOutput) { 0, NULL, 0 })

// when creating a BRTxOutput struct outside of a BRBitcoinTransaction, set address or script to NULL when done to free memory
size_t btcTxOutputAddress(const BRBitcoinTxOutput *output, char *address, size_t addrLen, BRAddressParams params);
void btcTxOutputSetAddress(BRBitcoinTxOutput *output, BRAddressParams params, const char *address);
void btcTxOutputSetScript(BRBitcoinTxOutput *output, const uint8_t *script, size_t scriptLen);

typedef struct {
    UInt256 txHash;
    UInt256 wtxHash;
    uint32_t version;
    BRBitcoinTxInput *inputs;
    size_t inCount;
    BRBitcoinTxOutput *outputs;
    size_t outCount;
    uint32_t lockTime;
    uint32_t blockHeight;
    uint32_t timestamp; // time interval since unix epoch
} BRBitcoinTransaction;

// returns a newly allocated empty transaction that must be freed by calling btcTransactionFree()
BRBitcoinTransaction *btcTransactionNew(void);

// returns a deep copy of tx and that must be freed by calling btcTransactionFree()
BRBitcoinTransaction *btcTransactionCopy(const BRBitcoinTransaction *tx);

// buf must contain a serialized tx
// retruns a transaction that must be freed by calling btcTransactionFree()
BRBitcoinTransaction *btcTransactionParse(const uint8_t *buf, size_t bufLen);

// returns number of bytes written to buf, or total bufLen needed if buf is NULL
// (tx->blockHeight and tx->timestamp are not serialized)
size_t btcTransactionSerialize(const BRBitcoinTransaction *tx, uint8_t *buf, size_t bufLen);

// adds an input to tx
void btcTransactionAddInput(BRBitcoinTransaction *tx, UInt256 txHash, uint32_t index, uint64_t amount,
                           const uint8_t *script, size_t scriptLen, const uint8_t *signature, size_t sigLen,
                           const uint8_t *witness, size_t witLen, uint32_t sequence);

// adds an output to tx
void btcTransactionAddOutput(BRBitcoinTransaction *tx, uint64_t amount, const uint8_t *script, size_t scriptLen);

// shuffles order of tx outputs
void btcTransactionShuffleOutputs(BRBitcoinTransaction *tx);

// size in bytes if signed, or estimated size assuming compact pubkey sigs
size_t btcTransactionSize(const BRBitcoinTransaction *tx);

// virtual transaction size as defined by BIP141: https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki
size_t btcTransactionVSize(const BRBitcoinTransaction *tx);

// minimum transaction fee needed for tx to relay across the bitcoin network (bitcoind 0.12 default min-relay fee-rate)
uint64_t btcTransactionStandardFee(const BRBitcoinTransaction *tx);

// checks if all signatures exist, but does not verify them
int btcTransactionIsSigned(const BRBitcoinTransaction *tx);

// adds signatures to any inputs with NULL signatures that can be signed with any keys
// forkId is 0 for bitcoin, 0x40 for b-cash, 0x4f for b-gold
// returns true if tx is signed
int btcTransactionSign(BRBitcoinTransaction *tx, int forkId, BRKey keys[], size_t keysCount);

// true if tx meets IsStandard() rules: https://bitcoin.org/en/developer-guide#standard-transactions
int btcTransactionIsStandard(const BRBitcoinTransaction *tx);

// returns a hash value for tx suitable for use in a hashtable
inline static size_t btcTransactionHash(const void *tx)
{
    return (size_t)((const BRBitcoinTransaction *)tx)->txHash.u32[0];
}

// true if tx and otherTx have equal txHash values
inline static int btcTransactionEq(const void *tx, const void *otherTx)
{
    return (tx == otherTx || UInt256Eq(((const BRBitcoinTransaction *)tx)->txHash, ((const BRBitcoinTransaction *)otherTx)->txHash));
}

// frees memory allocated for tx
void btcTransactionFree(BRBitcoinTransaction *tx);

#ifdef __cplusplus
}
#endif

#endif // BRTransaction_h
