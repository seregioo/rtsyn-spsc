/**
 * @file result/spsc.h
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Command-result SPSC queue.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */
#ifndef RTSYN_RESULT_SPSC_H
#define RTSYN_RESULT_SPSC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rtsyn/spsc/common.h"
#include "rtsyn/spsc/result/message.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RTSYN_SPSC_RESULT_CAPACITY 1024

typedef struct rtsyn_spsc_result_queue_e {
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE) uint64_t head;
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE) uint64_t tail;
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE)
    rtsyn_spsc_result_message_t buffer[RTSYN_SPSC_RESULT_CAPACITY];
} rtsyn_spsc_result_queue_t;

typedef struct rtsyn_spsc_result_shared_e {
    rtsyn_spsc_result_queue_t *queue;
    int fd;
    char name[RTSYN_SPSC_SHM_NAME_MAX];
} rtsyn_spsc_result_shared_t;

RTSYN_SPSC_STATIC_ASSERT((RTSYN_SPSC_RESULT_CAPACITY & (RTSYN_SPSC_RESULT_CAPACITY - 1)) == 0,
                         "RTSYN_SPSC_RESULT_CAPACITY must be a power of two");
RTSYN_SPSC_STATIC_ASSERT(RTSYN_SPSC_ATOMIC_U64_ALWAYS_LOCK_FREE,
                         "uint64_t atomics must be always lock-free for realtime SPSC cursors");
RTSYN_SPSC_STATIC_ASSERT(RTSYN_SPSC_ALIGNOF(rtsyn_spsc_result_queue_t)
                             == RTSYN_SPSC_CACHE_LINE_SIZE,
                         "result queue alignment ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_result_queue_t, head) == 0,
                         "result queue head offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_result_queue_t, tail)
                             == RTSYN_SPSC_CACHE_LINE_SIZE,
                         "result queue tail offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_result_queue_t, buffer)
                             == RTSYN_SPSC_CACHE_LINE_SIZE * 2,
                         "result queue buffer offset ABI changed");

void
rtsyn_spsc_result_init(rtsyn_spsc_result_queue_t *q);

bool
rtsyn_spsc_result_try_push(rtsyn_spsc_result_queue_t *q,
                           const rtsyn_spsc_result_message_t *msg);

bool
rtsyn_spsc_result_try_pop(rtsyn_spsc_result_queue_t *q, rtsyn_spsc_result_message_t *msg);

size_t
rtsyn_spsc_result_capacity(void);

size_t
rtsyn_spsc_result_size(const rtsyn_spsc_result_queue_t *q);

int
rtsyn_spsc_result_shared_create(rtsyn_spsc_result_shared_t *shared, const char *name);

int
rtsyn_spsc_result_shared_create_locked(rtsyn_spsc_result_shared_t *shared, const char *name);

int
rtsyn_spsc_result_shared_open(rtsyn_spsc_result_shared_t *shared, const char *name);

int
rtsyn_spsc_result_shared_open_locked(rtsyn_spsc_result_shared_t *shared, const char *name);

void
rtsyn_spsc_result_shared_close(rtsyn_spsc_result_shared_t *shared);

int
rtsyn_spsc_result_shared_unlink(const char *name);

#ifdef __cplusplus
}
#endif

#endif // RTSYN_RESULT_SPSC_H
