/**
 * @file spsc_telemetry.h
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Header file for the SPSC Telemetry
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */
#ifndef rtsyn_TELEMETRY_SPSC_H
#define rtsyn_TELEMETRY_SPSC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rtsyn/spsc/common.h"
#include "rtsyn/spsc/telemetry/message.h"

#define RTSYN_SPSC_TELEMETRY_CAPACITY 8192

#define SPSC_TELEMETRY_CAPACITY       RTSYN_SPSC_TELEMETRY_CAPACITY

/**
 * @brief Fixed-capacity telemetry event SPSC queue stored in shared memory.
 *
 * One realtime producer process may call @ref rtsyn_spsc_telemetry_try_push and
 * one telemetry consumer process may call @ref rtsyn_spsc_telemetry_try_pop.
 * The hot path does not allocate, lock, or perform syscalls after setup.
 */
typedef struct rtsyn_spsc_telemetry_queue_e {
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE) uint64_t head;
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE) uint64_t tail;
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE)
    rtsyn_spsc_telemetry_message_t buffer[RTSYN_SPSC_TELEMETRY_CAPACITY];
} rtsyn_spsc_telemetry_queue_t;

/**
 * @brief Process-local handle for a mapped telemetry event queue.
 *
 * The handle itself is not shared; only @ref queue points into shared memory.
 */
typedef struct rtsyn_spsc_telemetry_shared_e {
    rtsyn_spsc_telemetry_queue_t *queue;
    int fd;
    char name[RTSYN_SPSC_SHM_NAME_MAX];
} rtsyn_spsc_telemetry_shared_t;

RTSYN_SPSC_STATIC_ASSERT((RTSYN_SPSC_TELEMETRY_CAPACITY & (RTSYN_SPSC_TELEMETRY_CAPACITY - 1)) == 0,
                         "RTSYN_SPSC_TELEMETRY_CAPACITY must be a power of two");
RTSYN_SPSC_STATIC_ASSERT(RTSYN_SPSC_ATOMIC_U64_ALWAYS_LOCK_FREE,
                         "uint64_t atomics must be always lock-free for realtime SPSC cursors");
RTSYN_SPSC_STATIC_ASSERT(RTSYN_SPSC_ALIGNOF(rtsyn_spsc_telemetry_queue_t)
                             == RTSYN_SPSC_CACHE_LINE_SIZE,
                         "telemetry queue alignment ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_queue_t, head) == 0,
                         "telemetry queue head offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_queue_t, tail)
                             == RTSYN_SPSC_CACHE_LINE_SIZE,
                         "telemetry queue tail offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_queue_t, buffer)
                             == RTSYN_SPSC_CACHE_LINE_SIZE * 2,
                         "telemetry queue buffer offset ABI changed");

/**
 * @brief Initialize an empty telemetry event queue.
 *
 * @param q Telemetry event queue to initialize. Passing NULL is allowed and has
 * no effect.
 */
void
rtsyn_spsc_telemetry_init(rtsyn_spsc_telemetry_queue_t *q);

/**
 * @brief Try to append one telemetry event.
 *
 * @param q Telemetry event queue owned by exactly one producer process.
 * @param msg Telemetry event to copy into the queue.
 * @return true when the event was written, false when arguments are invalid or
 * the event queue is full.
 *
 * @note A false result is non-blocking backpressure. The owner of the telemetry
 * SPSC must decide the policy: create/provision a larger or separate queue,
 * account for dropped telemetry outside this queue, or stall execution if loss
 * is not acceptable.
 */
bool
rtsyn_spsc_telemetry_try_push(rtsyn_spsc_telemetry_queue_t *q,
                              const rtsyn_spsc_telemetry_message_t *msg);

/**
 * @brief Try to read one telemetry event.
 *
 * @param q Telemetry event queue owned by exactly one consumer process.
 * @param msg Output event destination.
 * @return true when an event was read, false when arguments are invalid or the
 * event queue is empty.
 */
bool
rtsyn_spsc_telemetry_try_pop(rtsyn_spsc_telemetry_queue_t *q, rtsyn_spsc_telemetry_message_t *msg);

/**
 * @brief Publish a contiguous telemetry value range and its VALUES_WRITTEN event.
 *
 * The function first checks event-queue capacity, then writes all samples to the
 * values ring, then publishes one event referencing that value range.
 *
 * @param q Telemetry event queue owned by exactly one producer process.
 * @param values Telemetry values ring owned by the same producer process.
 * @param msg Event to publish. The function fills the VALUES_WRITTEN type,
 * values_start_index, and value_count fields before publishing.
 * @param samples Contiguous input samples to copy into @p values.
 * @param sample_count Number of samples to copy and reference from the event.
 * @return true when both the value range and event were written. Returns false
 * when arguments are invalid, the event queue is full, the values ring lacks
 * contiguous logical capacity for @p sample_count, or @p sample_count cannot fit
 * in the event's 32-bit value count.
 *
 * @note A false result is non-blocking backpressure. The owner of the telemetry
 * SPSC must decide the policy: create/provision a larger or separate telemetry
 * stream, account for dropped values/events outside this queue, or stall the
 * realtime execution if complete telemetry is mandatory.
 */
bool
rtsyn_spsc_telemetry_try_publish_values(rtsyn_spsc_telemetry_queue_t *q,
                                        rtsyn_spsc_telemetry_values_t *values,
                                        rtsyn_spsc_telemetry_message_t *msg,
                                        const rtsyn_spsc_telemetry_value_t *samples,
                                        size_t sample_count);

/**
 * @brief Return the fixed telemetry event queue capacity.
 *
 * @return Maximum number of queued telemetry events.
 */
size_t
rtsyn_spsc_telemetry_capacity(void);

/**
 * @brief Return the current telemetry event queue occupancy.
 *
 * @param q Telemetry event queue to inspect.
 * @return Number of queued telemetry events, or 0 when @p q is NULL.
 */
size_t
rtsyn_spsc_telemetry_size(const rtsyn_spsc_telemetry_queue_t *q);

/**
 * @brief Create and map a telemetry event queue shared-memory object.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_telemetry_shared_create(rtsyn_spsc_telemetry_shared_t *shared, const char *name);

/**
 * @brief Create, map, prefault, and lock a telemetry event queue.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 *
 * This setup API may call syscalls and can fail if the process lacks enough
 * locked-memory allowance. It is intended to run before entering realtime code.
 */
int
rtsyn_spsc_telemetry_shared_create_locked(rtsyn_spsc_telemetry_shared_t *shared, const char *name);

/**
 * @brief Open and map an existing telemetry event queue shared-memory object.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_telemetry_shared_open(rtsyn_spsc_telemetry_shared_t *shared, const char *name);

/**
 * @brief Open, map, prefault, and lock an existing telemetry event queue.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 *
 * This setup API may call syscalls and can fail if the process lacks enough
 * locked-memory allowance. It is intended to run before entering realtime code.
 */
int
rtsyn_spsc_telemetry_shared_open_locked(rtsyn_spsc_telemetry_shared_t *shared, const char *name);

/**
 * @brief Unmap and close a process-local telemetry event queue handle.
 *
 * @param shared Process-local handle to close. Passing NULL is allowed and has
 * no effect.
 */
void
rtsyn_spsc_telemetry_shared_close(rtsyn_spsc_telemetry_shared_t *shared);

/**
 * @brief Remove a telemetry event queue shared-memory object name.
 *
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_telemetry_shared_unlink(const char *name);

#endif // rtsyn_TELEMETRY_SPSC_H
