/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _OS_H
#define _OS_H

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef min
#define min(a, b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a, b) ((a)>(b)?(a):(b))
#endif

#include "syscfg/syscfg.h"
#include "nimble/nimble_npl.h"

#define OS_ALIGN(__n, __a) (                             \
        (((__n) & ((__a) - 1)) == 0)                   ? \
            (__n)                                      : \
            ((__n) + ((__a) - ((__n) & ((__a) - 1))))    \
        )
#define OS_ALIGNMENT    (BLE_NPL_OS_ALIGNMENT)

typedef uint32_t os_sr_t;
#define OS_ENTER_CRITICAL(_sr) (_sr = ble_npl_hw_enter_critical())
#define OS_EXIT_CRITICAL(_sr) (ble_npl_hw_exit_critical(_sr))
#define OS_ASSERT_CRITICAL() assert(ble_npl_hw_is_in_critical())

/* Mynewt components (not abstracted in NPL) */
#include "os/endian.h"
#include "os/queue.h"
#include "os/os_error.h"
#include "os/os_mbuf.h"
#include "os/os_mempool.h"
#include "esp_types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_err.h"
#include "esp32/rom/ets_sys.h"
typedef long os_time_t;
struct os_time {
	os_time_t sec;
	os_time_t usec;
};

/**
 * os_get_time - Get current time (sec, usec)
 * @t: Pointer to buffer for the time
 * Returns: 0 on success, -1 on failure
 */
int os_get_time(struct os_time *t);


/* Helper macros for handling struct os_time */

#define os_time_before(a, b) \
	((a)->sec < (b)->sec || \
	 ((a)->sec == (b)->sec && (a)->usec < (b)->usec))

#define os_time_sub(a, b, res) do { \
	(res)->sec = (a)->sec - (b)->sec; \
	(res)->usec = (a)->usec - (b)->usec; \
	if ((res)->usec < 0) { \
		(res)->sec--; \
		(res)->usec += 1000000; \
	} \
} while (0)



/**
 * os_get_random - Get cryptographically strong pseudo random data
 * @buf: Buffer for pseudo random data
 * @len: Length of the buffer
 * Returns: 0 on success, -1 on failure
 */
int os_get_random(unsigned char *buf, size_t len);

/**
 * os_random - Get pseudo random value (not necessarily very strong)
 * Returns: Pseudo random value
 */
unsigned long os_random(void);


#ifdef __cplusplus
}
#endif

#endif /* _OS_H */
