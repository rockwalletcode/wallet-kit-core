//
//  testTezos.c
//  WalletKitCore
//
//  Created by Ehsan Rezaie on 2020-06-17.
//  Copyright © 2020 Breadwinner AG. All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.
//
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include "support/BRArray.h"
#include "support/BRCrypto.h"
#include "support/BRBIP32Sequence.h"
#include "support/BRBIP39Mnemonic.h"
#include "support/BRBIP39WordsEn.h"
#include "support/BRKey.h"
#include "support/BRBase58.h"
#include "support/util/BRHex.h"

#include "tezos/BRTezosOperation.h"
#include "tezos/BRTezosTransaction.h"
#include "tezos/BRTezosAccount.h"

static int debug_log = 0;

static uint8_t char2int(char input)
{
    if(input >= '0' && input <= '9')
        return input - '0';
    if(input >= 'A' && input <= 'F')
        return input - 'A' + 10;
    if(input >= 'a' && input <= 'f')
        return input - 'a' + 10;
    return 0;
}

static void hex2bin(const char* src, uint8_t * target)
{
    while(*src && src[1])
    {
        *(target++) = (char2int(src[0]) << 4) | (char2int(src[1]) & 0x0f);
        src += 2;
    }
}

static void bin2HexString (uint8_t *input, size_t inputSize, char * output) {
    for (size_t i = 0; i < inputSize; i++) {
        sprintf(&output[i*2], "%02x", input[i]);
    }
}

static void printBytes(const char* message, uint8_t * bytes, size_t byteSize)
{
    if (message) printf("%s\n", message);
    for(int i = 0; i < byteSize; i++) {
        if (i >= 0 && i % 8 == 0) printf("\n");
        printf("%02X ", bytes[i]);
    }
    printf("\n");
}

static void printByteString(const char* message, uint8_t * bytes, size_t byteSize)
{
    if (message) printf("%s\n", message);
    for(int i = 0; i < byteSize; i++) {
        printf("%02X", bytes[i]);
    }
    printf("\n");
}

// MARK: - Account Tests

typedef struct {
    const char * paperKey;
    const char * pubKey;
    const char * address;
} TestAccount;

TestAccount testAccount1 = {
    "patient doctor olympic frog force glimpse endless antenna online dragon bargain someone",
    "efc82a1445744a87fec55fce35e1b7ec80f9bbed9df2a03bcdde1a346f3d4294",
    "tz1SeV3tueHQMTfquZSU7y98otvQTw6GDKaY"
};

TestAccount testAccount2 = {
    "boring head harsh green empty clip fatal typical found crane dinner timber",
    "efc82a1445744a87fec55fce35e1b7ec80f9bbed9df2a03bcdde1a346f3d4294",
    "tz1PTZ7kd7BwpB9sNuMgJrwksEiYX3fb9Bdf"
};

// caller must free
static BRTezosAccount
makeAccount(TestAccount accountInfo) {
    UInt512 seed = UINT512_ZERO;
    BRBIP39DeriveKey(seed.u8, accountInfo.paperKey, NULL); // no passphrase
    BRTezosAccount account = tezosAccountCreateWithSeed(seed);
    return account;
}

static UInt512
getSeed(TestAccount account) {
    UInt512 seed = UINT512_ZERO;
    BRBIP39DeriveKey(seed.u8, account.paperKey, NULL);
    return seed;
}

static void
testCreateTezosAccountWithSeed() {
    BRTezosAccount account = makeAccount(testAccount1);
    assert(account);
    
    uint8_t expectedPubKey[32];
    hex2bin(testAccount1.pubKey, expectedPubKey);
    BRTezosPublicKey publicKey = tezosAccountGetPublicKey(account);
    assert (0 == (memcmp(expectedPubKey, publicKey.bytes, 32)));
    
    BRTezosAddress address = tezosAccountGetAddress (account);
    
    const char * expectedAddress = testAccount1.address;
    const char * accountAddress = tezosAddressAsString(address);
    assert (0 == strcmp (expectedAddress, accountAddress));

    BRTezosAddress addressFromString = tezosAddressCreateFromString(expectedAddress, true);
    assert (1 == tezosAddressEqual(address, addressFromString));
}

static void
testCreateTezosAccountWithSerializedAccount() {
    BRTezosAccount account = makeAccount(testAccount1);
    assert(account);
    
    // Serialize the account
    size_t bytesCount = 0;
    uint8_t * serializedAccount = tezosAccountGetSerialization(account, &bytesCount);
    assert(32 == bytesCount);
    assert(serializedAccount);
    
    // Create a new account with the serialized bytes
    BRTezosAccount account2 = tezosAccountCreateWithSerialization(serializedAccount, bytesCount);
    assert(account2);
    
    BRTezosAddress expectedAddress = tezosAddressCreateFromString(testAccount1.address, true);
    BRTezosAddress account2Address = tezosAccountGetAddress(account2);
    
    assert(1 == tezosAddressEqual(expectedAddress, account2Address));
    
    tezosAddressFree(expectedAddress);
    tezosAddressFree(account2Address);
    tezosAccountFree(account);
    tezosAccountFree(account2);
    free(serializedAccount);
}

// MARK: - Address Tests

static void testAddressCreate() {
    // Prefixes
    assert(NULL != tezosAddressCreateFromString("tz1i5JJDhq7x8gVkpWq2Fwef3k7NEcBj2nJS", true));
    assert(NULL != tezosAddressCreateFromString("tz2TSvNTh2epDMhZHrw73nV9piBX7kLZ9K9m", true));
    assert(NULL != tezosAddressCreateFromString("tz3bEQoFCZEEfZMskefZ8q8e4eiHH1pssRax", true));
    assert(NULL == tezosAddressCreateFromString("tz4i5JJDhq7x8gVkpWq2Fwef3k7NEcBj2nJS", true));
    
    // Originating address
    assert(NULL != tezosAddressCreateFromString("KT1VG2WtYdSWz5E7chTeAdDPZNy2MpP8pTfL", true));
}

static void testAddressEqual() {
    BRTezosAddress a1 = tezosAddressCreateFromString("tz1i5JJDhq7x8gVkpWq2Fwef3k7NEcBj2nJS", true);
    BRTezosAddress a2 = tezosAddressClone (a1);
    BRTezosAddress a3 = tezosAddressCreateFromString("tz1i5JJDhq7x8gVkpWq2Fwef3k7NEcBj2nJS", true);
    assert(1 == tezosAddressEqual(a1, a2));
    assert(1 == tezosAddressEqual(a1, a3));

    // now check no equal
    BRTezosAddress a4 = tezosAddressCreateFromString("tz1Vs2z88hHRnFLss81M7dXHnbwhZNMDrSgD", true);
    assert(0 == tezosAddressEqual(a1, a4));

    tezosAddressFree (a1);
    tezosAddressFree (a2);
    tezosAddressFree (a3);
    tezosAddressFree (a4);
}

static void testAddressClone() {
    BRTezosAddress a1 = tezosAddressCreateFromString("tz1eEnQhbwf6trb8Q8mPb2RaPkNk2rN7BKi8", true);
    BRTezosAddress a2 = tezosAddressClone (a1);
    BRTezosAddress a3 = tezosAddressClone (a1);
    BRTezosAddress a4 = tezosAddressClone (a1);
    BRTezosAddress a5 = tezosAddressClone (a2);
    BRTezosAddress a6 = tezosAddressClone (a3);

    assert(1 == tezosAddressEqual(a1, a2));
    assert(1 == tezosAddressEqual(a1, a3));
    assert(1 == tezosAddressEqual(a1, a4));
    assert(1 == tezosAddressEqual(a1, a5));
    assert(1 == tezosAddressEqual(a1, a6));
    assert(1 == tezosAddressEqual(a2, a3));

    tezosAddressFree (a1);
    tezosAddressFree (a2);
    tezosAddressFree (a3);
    tezosAddressFree (a4);
    tezosAddressFree (a5);
    tezosAddressFree (a6);
}

static void testFeeAddress() {
    BRTezosAddress feeAddress = tezosAddressCreateFromString("__fee__", false);
    assert (1 == tezosAddressIsFeeAddress(feeAddress));
    char * feeAddressString = tezosAddressAsString (feeAddress);
    assert(0 == strcmp(feeAddressString, "__fee__"));
    free (feeAddressString);
    tezosAddressFree(feeAddress);

    BRTezosAddress address = tezosAddressCreateFromString("tz1eEnQhbwf6trb8Q8mPb2RaPkNk2rN7BKi8", true);
    assert (0 == tezosAddressIsFeeAddress(address));
    tezosAddressFree(address);
    
    assert(NULL == tezosAddressCreateFromString("__fee__", true));
}

void testUnknownAddress() {
    BRTezosAddress address = tezosAddressCreateFromString("unknown", false);
    assert(address);

    char * addressString = tezosAddressAsString(address);
    assert (strcmp(addressString, "unknown") == 0);
    free (addressString);
    tezosAddressFree(address);
    
    assert(NULL == tezosAddressCreateFromString("unknown", true));
}

// MARK: - Transaction Tests



// MARK: - Wallet Tests

static void testCreateWallet() {
    BRTezosAccount account = makeAccount(testAccount1);

    BRTezosAddress expectedAddress = tezosAddressCreateFromString(testAccount1.address, true);

    BRTezosAddress sourceAddress = tezosAccountGetAddress(account);
    assert(tezosAddressEqual(sourceAddress, expectedAddress) == 1);

    tezosAccountFree(account);
    tezosAddressFree (expectedAddress);
    tezosAddressFree (sourceAddress);
}

static void testWalletBalance() {
//    BRTezosAccount account = makeAccount(testAccount1);
//    BRTezosUnitMutez expectedBalance = 0;
    
//    assert(expectedBalance == tezosWalletGetBalance(wallet));
//    
//    tezosWalletSetBalance(wallet, 250000);
//    assert(250000 == tezosWalletGetBalance(wallet));
//    
//    tezosWalletSetBalance(wallet, 0);
    
//    BRTezosAddress sourceAddress = tezosAddressCreateFromString("tz1eEnQhbwf6trb8Q8mPb2RaPkNk2rN7BKi8", true);
//    BRTezosAddress targetAddress = tezosAccountGetAddress(wallet);
//
//    BRTezosHash hash1, hash2, hash3, hash4;
//
//    BRBase58Decode(hash1.bytes, sizeof(hash1.bytes), "oot76ue8Jwyo5L6FgdiqyAPdDWAv84hXXkCi7oroJeDFXRbc298");
//    BRBase58Decode(hash1.bytes, sizeof(hash1.bytes), "ooSVYUdKnRA1yTwHdB7MqH5iXWBdscgtxfDdDAWxK8iR7K6rtY3");
//    BRBase58Decode(hash1.bytes, sizeof(hash1.bytes), "opT4Kb1WyijRnCakEy8PHX87KYHJpP2WA3ciLfmZqmRi4CTafCb");
//    BRBase58Decode(hash1.bytes, sizeof(hash1.bytes), "ooYSWSUBhtUZkNUzGbMHDYjiSKUDu4TUZKEW3rNHG69hMEN16Tc");
//
//    // incoming
//    BRTezosTransfer tf1 = tezosTransferCreate(sourceAddress, targetAddress, 100000000, 10000, hash1, 0, 1, 0);
//    BRTezosTransfer tf2 = tezosTransferCreate(sourceAddress, targetAddress, 200000000, 5000, hash2, 1, 1, 0);
//    BRTezosTransfer tf3 = tezosTransferCreate(sourceAddress, targetAddress, 300000000, 5000, hash3, 2, 1, 0);
//    expectedBalance = 100000000L + 200000000L + 300000000L;
//
//    tezosWalletAddTransfer(wallet, tf1);
//    tezosWalletAddTransfer(wallet, tf2);
//    tezosWalletAddTransfer(wallet, tf3);
//    assert(expectedBalance == tezosWalletGetBalance(wallet));
//
//    // outgoing
//    BRTezosTransfer tf4 = tezosTransferCreate(targetAddress, sourceAddress, 50000000, 5000, hash4, 3, 1, 0);
//
//    expectedBalance -= (50000000L + 5000L);
//
//    tezosWalletAddTransfer(wallet, tf4);
//    assert(expectedBalance == tezosWalletGetBalance(wallet));
//
//    tezosTransferFree(tf1);
//    tezosTransferFree(tf2);
//    tezosTransferFree(tf3);
//    tezosTransferFree(tf4);
//
//    tezosWalletFree(wallet);
//    tezosAccountFree(account);
}

// MARK: - Encoder Tests

extern BRData
encodeZarith (int64_t value);

static void
testZarithNumberEncode(int64_t input, const char* expectedHex) {
    BRData encoded = encodeZarith(input);
    char encodedOutput[64] = {0};
    bin2HexString(encoded.bytes, encoded.size, encodedOutput);
//    printf("input: %" PRIx64 "\n", input);
//    printByteString(0, encoded.bytes, encoded.size);
    assert (0 == strcasecmp(expectedHex, encodedOutput));
    dataFree(encoded);
}

static void
testEncodeZarith() {
    testZarithNumberEncode(0x04fa, "fa09");
    testZarithNumberEncode(0x27d8, "d84f");
    testZarithNumberEncode(0x02540be400, "80c8afa025");
    testZarithNumberEncode(0x2710, "904e");
    testZarithNumberEncode(0x115, "9502");
    testZarithNumberEncode(0x3b9aca00, "8094ebdc03");
    testZarithNumberEncode(0xadd9, "d9db02");
    testZarithNumberEncode(0x1fffffffffffff, "ffffffffffffff0f");
    testZarithNumberEncode(0x20000000000000, "8080808080808010");
    testZarithNumberEncode(0x20000000000001, "8180808080808010");
    testZarithNumberEncode(0x20000000000002, "8280808080808010");
}

// MARK: - Transaction Tests

static void
testTransactionSerialize() {
    BRTezosAccount account = makeAccount(testAccount1);
    BRTezosAddress sourceAddress;
    BRTezosAddress targetAddress;
    BRTezosOperationFeeBasis operationFeeBasis;

    BRTezosOperation   operation;
    BRTezosTransaction transaction;

    BRData unsignedBytes;
    BRTezosPublicKey publicKey;

    int64_t amount;
    int64_t counter;
    
    char * unsignedBytesInHex;

    // transaction
    sourceAddress = tezosAddressCreateFromString("tz1SeV3tueHQMTfquZSU7y98otvQTw6GDKaY", true);
    targetAddress = tezosAddressCreateFromString("tz1es8RjqHUD483BN9APWtvCzgjTFVGeMh3y", true);
    counter = 3;
    amount = 100000000;

    operationFeeBasis = tezosOperationFeeBasisCreate (TEZOS_OP_TRANSACTION, 52500, 10200, 0, 0, counter, 0);  // mutez/kbyte = 48800

    BRTezosHash lastBlockHash;
    BRBase58CheckDecode(lastBlockHash.bytes, sizeof(lastBlockHash.bytes), "BMZck1BxBCkFHJNSDp6GZBYsawi5U6cQYdzipKK7EUTZCrsG74s");

    operation     = tezosOperationCreateTransaction(sourceAddress, targetAddress, operationFeeBasis, amount);
    unsignedBytes = tezosOperationSerializeList (&operation, 1, lastBlockHash);
    unsignedBytesInHex = hexEncodeCreate (NULL, unsignedBytes.bytes,unsignedBytes.size);
    //printByteString(0, unsignedBytes.bytes, unsignedBytes.size);
    assert (0 == strcasecmp("f3b761a633b2b0cc9d2edbb09cda4800818f893b3d6567b09a818f1a5f685fb86c004cdee21a9180f80956ab8d27fb6abdbd89934052949a0303d84fac0280c2d72f0000d2e495a7ab40156d0a7c35b73d2530a3470fc87000", unsignedBytesInHex));

    free (unsignedBytesInHex);
    dataFree(unsignedBytes);
    
    // reveal
    operationFeeBasis = tezosOperationFeeBasisCreate (TEZOS_OP_REVEAL, 52500, 10200, 0, 0, counter, 0);  // mutez/kbyte = 48800

    publicKey     = tezosAccountGetPublicKey(account);
    operation     = tezosOperationCreateReveal(sourceAddress, targetAddress, operationFeeBasis, publicKey);
    unsignedBytes = tezosOperationSerializeList (&operation, 1, lastBlockHash);
    unsignedBytesInHex = hexEncodeCreate (NULL, unsignedBytes.bytes,unsignedBytes.size);
    //printByteString(0, unsignedBytes.bytes, unsignedBytes.size);
    assert (0 == strcasecmp("f3b761a633b2b0cc9d2edbb09cda4800818f893b3d6567b09a818f1a5f685fb86b004cdee21a9180f80956ab8d27fb6abdbd89934052949a0303d84fac0200efc82a1445744a87fec55fce35e1b7ec80f9bbed9df2a03bcdde1a346f3d4294", unsignedBytesInHex));

    free (unsignedBytesInHex);
    dataFree(unsignedBytes);

    // delegation on
    operationFeeBasis = tezosOperationFeeBasisCreate (TEZOS_OP_DELEGATION, 52500, 10200, 0, 0, counter, 0);  // mutez/kbyte = 48800

    tezosAddressFree(targetAddress);
    targetAddress = tezosAddressCreateFromString("tz1RKLoYm4vtLzo7TAgGifMDAkiWhjfyXwP4", true);
    operation     = tezosOperationCreateDelegation (sourceAddress, targetAddress, operationFeeBasis);
    unsignedBytes = tezosOperationSerializeList (&operation, 1, lastBlockHash);
    unsignedBytesInHex = hexEncodeCreate (NULL, unsignedBytes.bytes,unsignedBytes.size);
    //printByteString(0, unsignedBytes.bytes, unsignedBytes.size);
    assert (0 == strcasecmp("f3b761a633b2b0cc9d2edbb09cda4800818f893b3d6567b09a818f1a5f685fb86e004cdee21a9180f80956ab8d27fb6abdbd89934052949a0303d84fac02ff003e47f837f0467b4acde406ed5842f35e2414b1a8", unsignedBytesInHex));

    free (unsignedBytesInHex);
    dataFree(unsignedBytes);

    // delegation off
    tezosAddressFree(targetAddress);
    operation     = tezosOperationCreateDelegation (sourceAddress, sourceAddress, operationFeeBasis);
    unsignedBytes = tezosOperationSerializeList (&operation, 1, lastBlockHash);
    unsignedBytesInHex = hexEncodeCreate (NULL, unsignedBytes.bytes,unsignedBytes.size);
    //printByteString(0, unsignedBytes.bytes, unsignedBytes.size);
    assert (0 == strcasecmp("f3b761a633b2b0cc9d2edbb09cda4800818f893b3d6567b09a818f1a5f685fb86e004cdee21a9180f80956ab8d27fb6abdbd89934052949a0303d84fac0200", unsignedBytesInHex));

    free (unsignedBytesInHex);
    dataFree(unsignedBytes);


    tezosAddressFree(sourceAddress);
    tezosAccountFree(account);
}

static void
testBatchOperationSerialize() {
    BRTezosAccount account = makeAccount(testAccount1);
    BRTezosAddress sourceAddress;
    BRTezosAddress targetAddress;
    int64_t amount;
    int64_t counter;
    
    char serializedHex[512] = {0};
    
    sourceAddress = tezosAddressCreateFromString("tz1SeV3tueHQMTfquZSU7y98otvQTw6GDKaY", true);
    targetAddress = tezosAddressCreateFromString("tz1es8RjqHUD483BN9APWtvCzgjTFVGeMh3y", true);
    counter = 3;
    amount = 100000000;

    BRTezosHash lastBlockHash;
    BRBase58CheckDecode(lastBlockHash.bytes, sizeof(lastBlockHash.bytes), "BMZck1BxBCkFHJNSDp6GZBYsawi5U6cQYdzipKK7EUTZCrsG74s");

    BRTezosPublicKey publicKey = tezosAccountGetPublicKey(account);

    // reveal op
    BRTezosOperationFeeBasis revealFeeBasis  = tezosOperationFeeBasisCreate (TEZOS_OP_REVEAL, 52500, 10200, 0, 0, counter, 0);  // mutez/kbyte = 48800
    BRTezosOperation         revealOperation = tezosOperationCreateReveal (sourceAddress, targetAddress, revealFeeBasis, publicKey);
    
    // transaction op
    BRTezosOperationFeeBasis transferFeeBasis  = tezosOperationFeeBasisCreate (TEZOS_OP_TRANSACTION, 52500, 10200, 0, 0, counter, 0);  // mutez/kbyte = 48800
    BRTezosOperation         transferOperation = tezosOperationCreateTransaction (sourceAddress, targetAddress, transferFeeBasis, amount);

    BRTezosOperation operations[2] = {
        revealOperation,
        transferOperation
    };

    BRData unsignedBytes     = tezosOperationSerializeList (operations, 2, lastBlockHash);
    char *unsignedBytesInHex = hexEncodeCreate (NULL, unsignedBytes.bytes,unsignedBytes.size);
//    //printByteString(0, unsignedBytes.bytes, unsignedBytes.size);
    assert (0 == strcasecmp("f3b761a633b2b0cc9d2edbb09cda4800818f893b3d6567b09a818f1a5f685fb86b004cdee21a9180f80956ab8d27fb6abdbd89934052949a0303d84fac0200efc82a1445744a87fec55fce35e1b7ec80f9bbed9df2a03bcdde1a346f3d42946c004cdee21a9180f80956ab8d27fb6abdbd89934052949a0303d84fac0280c2d72f0000d2e495a7ab40156d0a7c35b73d2530a3470fc87000", unsignedBytesInHex));

    free (unsignedBytesInHex);
    dataFree(unsignedBytes);

    tezosAddressFree(targetAddress);
    tezosAddressFree(sourceAddress);
    tezosAccountFree(account);
}

static void
testTransactionSign() {
    BRTezosAccount account = makeAccount(testAccount1);
    UInt512 seed = getSeed(testAccount1);
    BRTezosAddress sourceAddress;
    BRTezosAddress targetAddress;
    BRTezosOperationFeeBasis operationFeeBasis;
    int64_t amount;
    int64_t counter;

    // transaction
    sourceAddress = tezosAddressCreateFromString("tz1SeV3tueHQMTfquZSU7y98otvQTw6GDKaY", true);
    targetAddress = tezosAddressCreateFromString("tz1es8RjqHUD483BN9APWtvCzgjTFVGeMh3y", true);
    counter = 3;
    amount = 100000000;
    operationFeeBasis = tezosOperationFeeBasisCreate (TEZOS_OP_TRANSACTION, 52500, 10200, 0, 0, counter, 0);  // mutez/kbyte = 48800

    BRTezosHash lastBlockHash;
    BRBase58CheckDecode(lastBlockHash.bytes, sizeof(lastBlockHash.bytes), "BMZck1BxBCkFHJNSDp6GZBYsawi5U6cQYdzipKK7EUTZCrsG74s");

    BRTezosOperation   transferOperation = tezosOperationCreateTransaction (sourceAddress, targetAddress, operationFeeBasis, amount);
    BRTezosTransaction transaction       = tezosTransactionCreate (transferOperation);

    tezosTransactionSerializeAndSign (transaction, account, seed, lastBlockHash);

    BRData signedBytes;
    signedBytes.bytes = tezosTransactionGetSignedBytes(transaction, &signedBytes.size);
    //printByteString(0, signedBytes, signedSize);

    char *signedBytesInHex = hexEncodeCreate (NULL, signedBytes.bytes, signedBytes.size);
    assert (0 == strcasecmp("f3b761a633b2b0cc9d2edbb09cda4800818f893b3d6567b09a818f1a5f685fb86c004cdee21a9180f80956ab8d27fb6abdbd89934052949a0303d84fac0280c2d72f0000d2e495a7ab40156d0a7c35b73d2530a3470fc87000333955b5c77d6c054dd9cad5359b57f7c0990932bf36e957604762e03d4a18364c88ff2785dba99e4899d3f5d84f93507f5ef60f000e9b8b84189d49dd975004", signedBytesInHex));

    BRTezosHash hash = tezosTransactionGetHash(transaction);
    char hashString[64] = {0};
    BRBase58CheckEncode(hashString, sizeof(hashString), hash.bytes, sizeof(hash.bytes));
    assert (0 == strcmp ("onwgTQgCHBPvTGFWmGrXzDMm3HQdJ4bvWwvv6LEEExmKy6CwMoo", hashString));
    
    tezosAddressFree(targetAddress);
    tezosAddressFree(sourceAddress);
    tezosTransactionFree(transaction);
    tezosAccountFree(account);
}

static void
testTransactionSignWithReveal() {
    BRTezosAccount account = makeAccount(testAccount2);
    UInt512 seed = getSeed(testAccount2);
    BRTezosAddress sourceAddress;
    BRTezosAddress targetAddress;
    int64_t amount;
    int64_t counter;

    // transaction
    sourceAddress = tezosAddressCreateFromString("tz1PTZ7kd7BwpB9sNuMgJrwksEiYX3fb9Bdf", true);
    targetAddress = tezosAddressCreateFromString("tz1YZpECan19MCZpubtM4zo4mgURHaLoMomy", true);
    counter = 6307075;
    amount = 100000;


    BRTezosPublicKey publicKey = tezosAccountGetPublicKey(account);

    BRTezosHash lastBlockHash;
    BRBase58CheckDecode(lastBlockHash.bytes, sizeof(lastBlockHash.bytes), "BLcz2Y6BikLFrwnejtRgBPSiGt1RLTjizUCg15BsUZ6x6JFazJS");

    BRTezosOperationFeeBasis revealFeeBasis  = tezosOperationFeeBasisCreate (TEZOS_OP_REVEAL,      0, 24000, 0, 0, counter,     0);  // mutez/kbyte = 48800
    BRTezosOperation         revealOperation = tezosOperationCreateReveal   (sourceAddress, targetAddress, revealFeeBasis, publicKey);

    BRTezosOperationFeeBasis transferFeeBasis  = tezosOperationFeeBasisCreate    (TEZOS_OP_TRANSACTION, 10500, 24000, 0, 0, counter + 1, 0);  // mutez/kbyte = 48800
    BRTezosOperation         transferOperation = tezosOperationCreateTransaction (sourceAddress, targetAddress, transferFeeBasis, amount);

    BRTezosTransaction transaction       = tezosTransactionCreateWithReveal (transferOperation, revealOperation);

    tezosTransactionSerializeAndSign (transaction, account, seed, lastBlockHash);

    BRData signedBytes;
    signedBytes.bytes = tezosTransactionGetSignedBytes(transaction, &signedBytes.size);

    char *signedBytesInHex = hexEncodeCreate (NULL, signedBytes.bytes, signedBytes.size);
    assert (0 == strcasecmp("77aa56c6022b22922cc1e5760ff22768437341b41f6f084b14a8d2487c80b7a86b0029e55328366cf257b64de39e784c9b6682c2f2b50083fa8003c0bb01ac020064b6cfc1ed37bc26ab4c68ec93d4769f98e83f1e07afd36fb4cb42d01203339e6c0029e55328366cf257b64de39e784c9b6682c2f2b5845284fa8003c0bb01ac02a08d0600008dcd911b4896ac05a3649d4cd1c462cef4e7f645007120555278435abcd9a8b1397d1964024acd978ee6286b34a04d18aa0d8ee00bf911bc10879bcac6dd9198189f39ee80246724750ac8f3ccba042c3d1fd45507", signedBytesInHex));

    BRTezosHash hash = tezosTransactionGetHash (transaction);
    char hashString[64] = {0};
    BRBase58CheckEncode(hashString, sizeof(hashString), hash.bytes, sizeof(hash.bytes));
    assert (0 == strcmp ("op5o529Ggohc614wQBKfxsQXG6tcH5Q6jqDSDZ4dVDz8mC6LGDB", hashString));
    
    tezosAddressFree(targetAddress);
    tezosAddressFree(sourceAddress);
    tezosTransactionFree(transaction);
    tezosAccountFree(account);
}

// MARK: -

static void
tezosAccountTests() {
    testCreateTezosAccountWithSeed();
    testCreateTezosAccountWithSerializedAccount();
}

static void
tezosAddressTests() {
    testAddressCreate();
    testAddressEqual();
    testAddressClone();
    testFeeAddress();
    testUnknownAddress();
}

static void
tezosTransactionTests() {
    testTransactionSerialize();
    testBatchOperationSerialize();
    testTransactionSign();
    testTransactionSignWithReveal();
}

static void
tezosWalletTests() {
    testCreateWallet();
    testWalletBalance();
}

static void
tezosEncoderTests() {
    testEncodeZarith();
}

// MARK: -

extern void
runTezosTest (void /* ... */) {
    printf("Running tezos unit tests...\n");
    tezosAccountTests();
    tezosAddressTests();
    tezosEncoderTests();
    tezosTransactionTests();
    tezosWalletTests();
}

