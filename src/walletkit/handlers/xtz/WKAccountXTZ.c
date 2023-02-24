//
//  WKAccountXTZ.c
//  WalletKitCore
//
//  Created by Bryan Goring on 06/15/2021.
//  Copyright © 2019 Breadwinner AG. All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.
//
#include "WKXTZ.h"

static WKAccountDetails
wkAccountCreateFromSeedXTZ(
    WKBoolean   isMainnet,
    UInt512     seed    ) {
    return tezosAccountCreateWithSeed(seed);
}

static WKAccountDetails
wkAccountCreateFromBytesXTZ(
    uint8_t*    bytes,
    size_t      len ) {

    BRTezosAccount xtz = tezosAccountCreateWithSerialization(bytes, len);
    assert (NULL != xtz);

    return xtz;
}

static void
wkAccountReleaseXTZ(WKAccountDetails accountDetails) {
    tezosAccountFree ((BRTezosAccount)accountDetails);
}

static size_t
wkAccountSerializeXTZ(  uint8_t     *accountSerBuf,
                        WKAccount   account ) {

    assert (account != NULL);

    size_t          xtzSize;
    BRTezosAccount  xtzAcct;
    uint8_t         *xtzBytes;

    xtzAcct = (BRTezosAccount) wkAccountAs (account,
                                            WK_NETWORK_TYPE_XTZ);
    xtzBytes = tezosAccountGetSerialization (xtzAcct,
                                             &xtzSize );

    if (accountSerBuf != NULL) {
        memcpy (accountSerBuf, xtzBytes, xtzSize);
    }

    free (xtzBytes);

    return xtzSize;
}

WKAccountHandlers wkAccountHandlersXTZ = {
    wkAccountCreateFromSeedXTZ,
    wkAccountCreateFromBytesXTZ,
    wkAccountReleaseXTZ,
    wkAccountSerializeXTZ
};



