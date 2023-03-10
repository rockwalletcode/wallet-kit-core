//
//  BBRUtilHex.c
//  WalletKitCore Ethereum
//
//  Created by Ed Gamble on 3/10/2018.
//  Copyright © 2018-2019 Breadwinner AG.  All rights reserved.
//
//  See the LICENSE file at the project root for license information.
//  See the CONTRIBUTORS file at the project root for a list of contributors.

#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "support/BRInt.h"
#include "BRHex.h"

// Convert a char into uint8_t (decode)
#define decodeChar(c)           ((uint8_t) _hexu(c))

// Convert a uint8_t into a char (encode)
#define encodeChar(u)           ((char)    _hexc(u))

extern void
hexDecode (uint8_t *target, size_t targetLen, const char *source, size_t sourceLen) {
    //
    assert (0 == sourceLen % 2);
    assert (2 * targetLen == sourceLen);
    
    for (int i = 0; i < targetLen; i++) {
        target[i] = (uint8_t) ((decodeChar(source[2*i]) << 4) | decodeChar(source[(2*i)+1]));
    }
}

extern size_t
hexDecodeLength (size_t stringLen) {
    assert (0 == stringLen % 2);
    return stringLen/2;
}

extern uint8_t *
hexDecodeCreate (size_t *targetLen, const char *source, size_t sourceLen) {
    size_t length = hexDecodeLength(sourceLen);
    if (NULL != targetLen) *targetLen = length;
    uint8_t *target = malloc (length);
    hexDecode (target, length, source, sourceLen);
    return target;
}

extern void
hexEncode (char *target, size_t targetLen, const uint8_t *source, size_t sourceLen) {
    assert (targetLen == 2 * sourceLen  + 1);
    
    for (int i = 0; i < sourceLen; i++) {
        target[2*i + 0] = encodeChar (source[i] >> 4);
        target[2*i + 1] = encodeChar (source[i]);
    }
    target[2*sourceLen] = '\0';
}

extern size_t
hexEncodeLength(size_t byteArrayLen) {
    return 2 * byteArrayLen + 1;
}

extern char *
hexEncodeCreate (size_t *targetLen, const uint8_t *source, size_t sourceLen) {
    size_t length = hexEncodeLength(sourceLen);
    if (NULL != targetLen) *targetLen = length;
    char *target = malloc (length);
    hexEncode(target, length, source, sourceLen);
    return target;
}

extern int
hexEncodeValidate (const char *number) {
    // Number contains only hex digits, has an even number and has at least two.
    if (NULL == number || '\0' == *number || 0 != strlen(number) % 2) return 0;

    while (*number)
        if (!isxdigit (*number++)) return 0;
    return 1;
}
