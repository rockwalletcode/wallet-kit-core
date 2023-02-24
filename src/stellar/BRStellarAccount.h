//
//  BRStellarAccount.h
//  WalletKitCore
//
//  Created by Carl Cherry on 5/21/2019.
//  Copyright © 2019 Breadwinner AG. All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.
//
#ifndef BRStellar_account_h
#define BRStellar_account_h

#ifdef __cplusplus
extern "C" {
#endif

#include "support/BRKey.h"
#include "BRStellarBase.h"
#include "BRStellarTransaction.h"
#include "BRStellarAddress.h"
#include "BRStellarFeeBasis.h"

typedef struct BRStellarAccountRecord *BRStellarAccount;

/**
 * Create a Stellar account object
 *
 * @param  paperKey  12 word mnemonic string
 * @return account
 */
extern BRStellarAccount /* caller must free - stellarAccountFree */
stellarAccountCreate (const char *paperKey);

/**
 * Create a Stellar account object
 *
 * @param  seed     512-bit secret
 * @return account
 */
extern BRStellarAccount /* caller must free - stellarAccountFree */
stellarAccountCreateWithSeed(UInt512 seed);

/**
 * Create a Stellar account object
 *
 * @param  key      BRKey with valid private key
 * @return account
 */
extern BRStellarAccount /* caller must free - stellarAccountFree */
stellarAccountCreateWithKey(BRKey key);


extern BRStellarAccount
stellarAccountCreateWithSerialization (uint8_t *bytes, size_t bytesCount);
extern uint8_t * // Caller owns memory and must delete calling "free"
stellarAccountGetSerialization (BRStellarAccount account, size_t *bytesCount);

/**
 * Free the memory for a stellar account object
 *
 * @param account BRStellarAccount to free
 *
 * @return void
 */
extern void stellarAccountFree(BRStellarAccount account);

/**
 * Serialize (and sign) a Stellar Transaction.  Ready for submission to the ledger.
 *
 * @param account         the account sending the payment
 * @param transaction     the transaction to serialize
 * @param paperKey        paper key of the sending account
 *
 * @return handle         BRStellarSerializedTransaction handle, use this to get the size and bytes
 *                        NOTE: If successful then the sequence number in the account is incremented
 *                        NOTE: is the handle is NULL then the serialization failed AND the sequence
 *                              was not incremented
 */
extern size_t
stellarAccountSignTransaction(BRStellarAccount account, BRStellarTransaction transaction, UInt512 seed);

/**
 * Get the stellar address for this account
 *
 * @param account   BRStellarAccount
 * @return address  the stellar address for this account
 */
extern BRStellarAddress
stellarAccountGetAddress(BRStellarAccount account);

extern int
stellarAccountHasAddress (BRStellarAccount account,
                         BRStellarAddress address);

/**
 * Get the stellar address for this account
 *
 * @param account     BRStellarAccount
 * @return accountID  raw account ID bytes
 */
extern BRStellarAccountID
stellarAccountGetAccountID(BRStellarAccount account);
    
/**
 * Get the account's primary address
 *
 * @param  account  the account
 * @return address  the primary stellar address for this account
 */
extern BRStellarAddress stellarAccountGetPrimaryAddress (BRStellarAccount account);

/**
 * Get the account's public key (for the primary address)
 *
 * @param  account     the account
 * @return publicKey   A BRKey object holding the public key
 */
extern BRKey stellarAccountGetPublicKey(BRStellarAccount account);

/**
 * Set the sequence number for this account
 *
 * The sequence number is used when sending transcations
 *
 * NOTE: The current transaction sequence number of the account. This number starts
 * equal to the ledger number at which the account was created, not at 0.
 *
 * @param account the account
 * @param sequence  a 64-bit unsigned number. It should be retrieved from network
 *                  or should be set to 1 more that the value in the lastest transaction
 */
extern void stellarAccountSetSequence(BRStellarAccount account, int64_t sequence);
extern void stellarAccountSetBlockNumberAtCreation(BRStellarAccount account, uint64_t blockNumber);

/**
 * Set the network type being used for this account
 *
 * The account will default to the production network if this fuction is not called
 *
 * @param account the account
 * @param networkType      STELLAR_NETWORK_PUBLIC or STELLAR_NETWORK_TESTNET
 */
extern void stellarAccountSetNetworkType(BRStellarAccount account, BRStellarNetworkType networkType);

/*
 * Get stellar account ID from string
 *
 * @param address     a string containing a valid Stellar account in base64 form. e.g.
 *                    GBKWF42EWZDRISFXW3V6WW5OTQOOZSJQ54UINC7CXN4.......
 * @return accountID  a BRStellarAccountID object
 */
extern BRStellarAccountID stellerAccountCreateStellarAccountID(const char * stellarAddress);

/**
 * Return the balance limit, either asMaximum or asMinimum
 *
 * @param account   tezos account
 * @param asMaximum - if true, return the wallet maximum limit; otherwise minimum limit
 * @param hasLimit  - must be non-NULL; assigns if wallet as the specified limit
 *
 * @return balance limit - in mutez units
 */
extern BRStellarAmount
stellarAccountGetBalanceLimit (BRStellarAccount account,
                            int asMaximum,
                            int *hasLimit);

extern BRStellarFeeBasis
stellarAccountGetDefaultFeeBasis (BRStellarAccount account);

#ifdef __cplusplus
}
#endif

#endif // BRStellar_account_h
