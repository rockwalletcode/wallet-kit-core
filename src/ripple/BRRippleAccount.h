//
//  BRRippleAccount.h
//  WalletKitCore
//
//  Created by Carl Cherry on 4/16/19.
//  Copyright © 2019 Breadwinner AG. All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.
//
#ifndef BRRipple_account_h
#define BRRipple_account_h

#include "BRRippleTransaction.h"
#include "support/BRKey.h"

typedef struct BRRippleAccountRecord *BRRippleAccount;


/**
 * Create a Ripple account object
 *
 * @param  paperKey  12 word mnemonic string
 * @return account
 */
extern BRRippleAccount /* caller must free - rippleAccountFree */
rippleAccountCreate (const char *paperKey);
/**
 * Create a Ripple account object
 *
 * @param  seed     512-bit secret
 * @return account
 */
extern BRRippleAccount /* caller must free - rippleAccountFree */
rippleAccountCreateWithSeed(UInt512 seed);

/**
 * Create a Ripple account object
 *
 * @param  key      BRKey with valid compressed public key
 * @return account
 */
extern BRRippleAccount /* caller must free - rippleAccountFree */
rippleAccountCreateWithKey(BRKey key);

/**
 * Create a Ripple account object from a (prior) serialization of the account
 *
 * @param bytes        raw bytes returned from rippleAccountGetSerialization
 * @param bytesCount   number of raw bytes
 * @return account     valid account object
 */
extern BRRippleAccount /* caller must free - rippleAccountFree */
rippleAccountCreateWithSerialization (uint8_t *bytes, size_t bytesCount);

/**
 * Free the memory for a ripple account object
 *
 * @param account BRRippleAccount to free
 *
 * @return void
 */
extern void rippleAccountFree(BRRippleAccount account);

/**
 * Set the sequence number
 *
 * @param sequence   uint32_t sequence number. The sequence number, relative to the initiating account,
 *                   of this transaction. A transaction is only valid if the Sequence number is exactly
 *                   1 greater than the previous transaction from the same account.
 *
 * @return void
 */
extern void rippleAccountSetSequence(BRRippleAccount account, BRRippleSequence sequence);

/**
 * Set the sequence number
 *
 * @param lastLedgerSequence    (Optional; strongly recommended) Highest ledger index this transaction
 *                              can appear in. Specifying this field places a strict upper limit on
 *                              how long the transaction can wait to be validated or rejected.
 *                              See Reliable Transaction Submission for more details.
 *
 * @return void
 */
extern void rippleAccountSetLastLedgerSequence(BRRippleAccount account,
                                               BRRippleLastLedgerSequence lastLedgerSequence);

/**
 * Creates a transaction that will close this account and send all possible XRP to the toAddress (minus the fee)
 *
 * @param account   the account to close
 * @param toAddress the account where any remaining XRP should be sent
 * @param feeBasis  the fee need to send this transaction
 *
 * @return void
 */
extern BRRippleTransaction
rippleAccountCreateCloseTransaction(BRRippleAccount account,
                                    BRRippleAddress toAddress,
                                    BRRippleFeeBasis feeBasis);

/**
 * Serialize (and sign) a Ripple Transaction.  Ready for submission to the ledger.
 *
 * @param account         the account sending the payment
 * @param transaction     the transaction to serialize
 * @param paperKey        paper key of the sending account
 *
 * @return size           size of serialized/siged bytes
 */
extern size_t
rippleAccountSignTransaction(BRRippleAccount account, BRRippleTransaction transaction, UInt512 seed);

/**
 * Get the account address
 *
 * @param account   - the account
 *
 * @return address  - a ripple address, caller owns object and must free with rippleAddressFree
 */
extern BRRippleAddress
rippleAccountGetAddress(BRRippleAccount account);

extern int
rippleAccountHasAddress (BRRippleAccount account,
                         BRRippleAddress address);

// Serialize `account`; return `bytes` and set `bytesCount`
extern uint8_t * /* caller must free - using "free" function */
rippleAccountGetSerialization (BRRippleAccount account, size_t *bytesCount);

/**
 * Get the account's primary address
 *
 * @param account the account
 *
 * @return address  - a ripple address, caller owns object and must free with rippleAddressFree
 */
extern BRRippleAddress rippleAccountGetPrimaryAddress (BRRippleAccount account);

extern BRKey rippleAccountGetPublicKey(BRRippleAccount account);

// Internal
extern BRRippleSequence rippleAccountGetSequence (BRRippleAccount account);

// Internal
extern void rippleAccountSetBlockNumberAtCreation (BRRippleAccount account, uint64_t blockHeight);

extern BRRippleUnitDrops
rippleAccountGetBalanceLimit (BRRippleAccount account,
                              int asMaximum,
                              int *hasLimit);

// The default fee for a transfer
extern BRRippleFeeBasis
rippleAccountGetDefaultFeeBasis (BRRippleAccount account);
// THe fee basis to delete an account
extern BRRippleFeeBasis
rippleAccountGetAccountDeleteFeeBasis (BRRippleAccount account);

#endif
