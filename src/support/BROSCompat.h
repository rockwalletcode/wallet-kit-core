//
//  BROSCompat.h
//
//  Created by Aaron Voisine on 2/14/20.
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

#ifndef BROSCompat_h
#define BROSCompat_h

#include <pthread.h>
#include <string.h>         // strlcpy()
#include <stdint.h>
#include <stdlib.h>         // arc4random
#include <stdio.h>          // FILE

#if defined (__linux__) && !defined(__ANDROID__)
// TODO: Including bsd/stdlib.h causes a `swift build` failure
//#include <bsd/stdlib.h>   // arc4random()
// TODO: <string.h> on Linux does not include strlcpy()
#include <bsd/string.h>     // strlcpy
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined (__linux__) && !defined(__ANDROID__)
    void arc4random_buf(void *_buf, size_t n);
    uint32_t arc4random_uniform(uint32_t upper_bound);

    extern int pthread_setname_np (pthread_t __target_thread, const char *__name)
    __THROW __nonnull ((2));


#if defined (__GNUC__)

// For missing ssize_t inclusion through stdlib.h w/o _XOPEN_SOURCE
#include <sys/types.h>

// For missing M_LN2 w/o _XOPEN_SOURCE
#define M_LN2          0.69314718055994530942  /* log_e 2 */

// For missing _tolower, _toupper defns
#define _tolower        tolower
#define _toupper        toupper

#endif

#endif // defined(__linux__)

#define PTHREAD_NULL   ((pthread_t) NULL)

typedef void* (*ThreadRoutine) (void*);         // pthread_create

extern int
pthread_setname_brd(pthread_t,  const char *name);

extern int
pthread_mutex_init_brd (pthread_mutex_t *mutex, int type);

extern void
pthread_yield_brd (void);

extern int
pthread_cond_timedwait_relative_brd (pthread_cond_t *cond,
                                     pthread_mutex_t *mutex,
                                     const struct timespec *reltime);

extern void
arc4random_buf_brd (void *bytes, size_t bytesCount);

extern uint32_t
arc4random_uniform_brd(uint32_t upperBbound);

extern int
mergesort_brd (void *__base, size_t __nel, size_t __width,
               int (*__compar)(const void *, const void *));

extern char*
strsep_brd(char **restrict stringp, const char *restrict delim);
    
extern FILE *
open_memstream_brd (char **bufp, size_t *sizep);

#ifdef __cplusplus
}
#endif

#endif // BROSCompat_h

