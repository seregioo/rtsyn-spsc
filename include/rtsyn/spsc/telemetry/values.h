/**
 * @file values.h
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Shared telemetry value ring for realtime output samples
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */
#ifndef RTSYN_SPSC_TELEMETRY_VALUES_H
#define RTSYN_SPSC_TELEMETRY_VALUES_H

#include <rtsyn/abi/value.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rtsyn/spsc/common.h"

#define RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY       65536
#define RTSYN_SPSC_TELEMETRY_VALUE_STRING_MAX_SIZE 16

#define RTSYN_SPSC_TELEMETRY_VALUE_KIND_NONE  0
#define RTSYN_SPSC_TELEMETRY_VALUE_KIND_PORT  1
#define RTSYN_SPSC_TELEMETRY_VALUE_KIND_STATE 2

/**
 * @brief Identifies the realtime node class that produced a telemetry value.
 *
 * @note This enum is stored inside the shared-memory IPC payload. Every process
 * using the queue must be built with a compatible C ABI for enum layout.
 */
typedef enum rtsyn_spsc_telemetry_source_e {
    RTSYN_SPSC_TELEMETRY_SOURCE_NONE = 0,
    RTSYN_SPSC_TELEMETRY_SOURCE_DEVICE,
    RTSYN_SPSC_TELEMETRY_SOURCE_PLUGIN,
    RTSYN_SPSC_TELEMETRY_SOURCE_MAX,
} rtsyn_spsc_telemetry_source_t;

/**
 * @brief Fixed-size telemetry sample written by a device or plugin operation.
 *
 * The realtime process writes one entry for each requested output value. Numeric
 * values are stored inline. String values are bounded by
 * @ref RTSYN_SPSC_TELEMETRY_VALUE_STRING_MAX_SIZE and must be null-terminated by
 * the producer when interpreted as C strings.
 *
 * @note This structure is part of the shared-memory IPC payload and contains C
 * enums. All C/C++ processes must use compatible enum layout. If another
 * language binding maps boolean values in this payload family, C stdbool must be
 * used, or a 1 byte size value if stdbool is not available.
 */
typedef struct rtsyn_spsc_telemetry_value_e {
    uint64_t cycle_id;
    uint64_t timestamp_ns;
    uint32_t node_id;
    uint32_t value_id;
    uint16_t sample_offset;
    uint16_t value_kind;
    rtsyn_spsc_telemetry_source_t source;
    rtsyn_abi_value_type_t value_type;

    union {
        float f32;
        double f64;
        int64_t i64;
        uint64_t u64;
        char string[RTSYN_SPSC_TELEMETRY_VALUE_STRING_MAX_SIZE];
    } data;
} rtsyn_spsc_telemetry_value_t;

/**
 * @brief SPSC ring containing telemetry values referenced by telemetry events.
 *
 * The producer is the realtime process. The consumer owns release progress by
 * calling @ref rtsyn_spsc_telemetry_values_release after it has copied or
 * otherwise consumed values.
 */
typedef struct rtsyn_spsc_telemetry_values_e {
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE) uint64_t head;
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE) uint64_t tail;
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE)
    rtsyn_spsc_telemetry_value_t buffer[RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY];
} rtsyn_spsc_telemetry_values_t;

/**
 * @brief Process-local handle for a mapped telemetry values ring.
 *
 * The handle itself is not shared; only @ref values points into shared memory.
 */
typedef struct rtsyn_spsc_telemetry_values_shared_e {
    rtsyn_spsc_telemetry_values_t *values;
    int fd;
    char name[RTSYN_SPSC_SHM_NAME_MAX];
} rtsyn_spsc_telemetry_values_shared_t;

RTSYN_SPSC_STATIC_ASSERT((RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY
                          & (RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY - 1))
                             == 0,
                         "RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY must be a power of two");
RTSYN_SPSC_STATIC_ASSERT(RTSYN_SPSC_ATOMIC_U64_ALWAYS_LOCK_FREE,
                         "uint64_t atomics must be always lock-free for realtime SPSC cursors");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_telemetry_source_t) == sizeof(uint32_t),
                         "telemetry source enum must be 4 bytes for IPC ABI");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_abi_value_type_t) == sizeof(uint32_t),
                         "ABI value type enum must be 4 bytes for telemetry IPC ABI");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_telemetry_value_t) == 56,
                         "telemetry value IPC ABI changed");
RTSYN_SPSC_STATIC_ASSERT(RTSYN_SPSC_ALIGNOF(rtsyn_spsc_telemetry_value_t) == 8,
                         "telemetry value alignment ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_value_t, cycle_id) == 0,
                         "telemetry value cycle_id offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_value_t, timestamp_ns) == 8,
                         "telemetry value timestamp offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_value_t, node_id) == 16,
                         "telemetry value node_id offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_value_t, value_id) == 20,
                         "telemetry value value_id offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_value_t, sample_offset) == 24,
                         "telemetry value sample_offset offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_value_t, value_kind) == 26,
                         "telemetry value kind offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_value_t, source) == 28,
                         "telemetry value source offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_value_t, value_type) == 32,
                         "telemetry value type offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_value_t, data) == 40,
                         "telemetry value data offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(RTSYN_SPSC_ALIGNOF(rtsyn_spsc_telemetry_values_t)
                             == RTSYN_SPSC_CACHE_LINE_SIZE,
                         "telemetry values ring alignment ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_values_t, head) == 0,
                         "telemetry values ring head offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_values_t, tail)
                             == RTSYN_SPSC_CACHE_LINE_SIZE,
                         "telemetry values ring tail offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_values_t, buffer)
                             == RTSYN_SPSC_CACHE_LINE_SIZE * 2,
                         "telemetry values ring buffer offset ABI changed");

/**
 * @brief Initialize an empty telemetry values ring.
 *
 * @param values Telemetry values ring to initialize. Passing NULL is allowed
 * and has no effect.
 */
void
rtsyn_spsc_telemetry_values_init(rtsyn_spsc_telemetry_values_t *values);

/**
 * @brief Return the fixed telemetry values ring capacity.
 *
 * @return Maximum number of retained telemetry value samples.
 */
size_t
rtsyn_spsc_telemetry_values_capacity(void);

/**
 * @brief Return the current telemetry values ring occupancy.
 *
 * @param values Telemetry values ring to inspect.
 * @return Number of retained samples, or 0 when @p values is NULL.
 */
size_t
rtsyn_spsc_telemetry_values_size(const rtsyn_spsc_telemetry_values_t *values);

/**
 * @brief Return the current telemetry values ring free space.
 *
 * @param values Telemetry values ring to inspect.
 * @return Number of samples that can currently be appended. Passing NULL
 * returns the full capacity because the null queue has size 0.
 */
size_t
rtsyn_spsc_telemetry_values_free_space(const rtsyn_spsc_telemetry_values_t *values);

/**
 * @brief Try to append a contiguous logical range of telemetry samples.
 *
 * When successful, @p start_index receives the first logical index of the range
 * if it is not NULL. The consumer can later read each sample with
 * @ref rtsyn_spsc_telemetry_values_try_get until the range is released.
 *
 * @param values Telemetry values ring owned by exactly one producer process.
 * @param samples Contiguous input samples to copy into the values ring.
 * @param sample_count Number of samples to copy.
 * @param start_index Optional output for the first logical index written.
 * @return true when all samples were written, false when arguments are invalid
 * or the values ring is full.
 *
 * @note A false result is non-blocking backpressure. The owner of the telemetry
 * SPSC must decide the policy: create/provision a larger or separate values
 * ring, account for dropped values outside this queue, or stall the realtime
 * execution if complete telemetry is mandatory.
 */
bool
rtsyn_spsc_telemetry_values_try_push(rtsyn_spsc_telemetry_values_t *values,
                                     const rtsyn_spsc_telemetry_value_t *samples,
                                     size_t sample_count, uint64_t *start_index);

/**
 * @brief Try to read a telemetry sample by its logical ring index.
 *
 * @param values Telemetry values ring to inspect.
 * @param index Logical ring index to read.
 * @param sample Output sample destination.
 * @return true when @p index is still retained in the values ring, false when
 * arguments are invalid or the value has not been written or was already
 * released.
 */
bool
rtsyn_spsc_telemetry_values_try_get(const rtsyn_spsc_telemetry_values_t *values, uint64_t index,
                                    rtsyn_spsc_telemetry_value_t *sample);

/**
 * @brief Release consumed telemetry samples from the values ring.
 *
 * @param values Telemetry values ring owned by exactly one consumer process.
 * @param sample_count Number of consumed samples to release.
 * @return true when the samples were released, false when arguments are invalid
 * or @p sample_count exceeds the number of retained samples.
 *
 * Only the consumer process may call this function.
 */
bool
rtsyn_spsc_telemetry_values_release(rtsyn_spsc_telemetry_values_t *values, size_t sample_count);

/**
 * @brief Create and map a telemetry values ring shared-memory object.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_telemetry_values_shared_create(rtsyn_spsc_telemetry_values_shared_t *shared,
                                          const char *name);

/**
 * @brief Create, map, prefault, and lock a telemetry values ring.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 *
 * This setup API may call syscalls and can fail if the process lacks enough
 * locked-memory allowance. It is intended to run before entering realtime code.
 */
int
rtsyn_spsc_telemetry_values_shared_create_locked(rtsyn_spsc_telemetry_values_shared_t *shared,
                                                 const char *name);

/**
 * @brief Open and map an existing telemetry values ring shared-memory object.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_telemetry_values_shared_open(rtsyn_spsc_telemetry_values_shared_t *shared,
                                        const char *name);

/**
 * @brief Open, map, prefault, and lock an existing telemetry values ring.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 *
 * This setup API may call syscalls and can fail if the process lacks enough
 * locked-memory allowance. It is intended to run before entering realtime code.
 */
int
rtsyn_spsc_telemetry_values_shared_open_locked(rtsyn_spsc_telemetry_values_shared_t *shared,
                                               const char *name);

/**
 * @brief Unmap and close a process-local telemetry values ring handle.
 *
 * @param shared Process-local handle to close. Passing NULL is allowed and has
 * no effect.
 */
void
rtsyn_spsc_telemetry_values_shared_close(rtsyn_spsc_telemetry_values_shared_t *shared);

/**
 * @brief Remove a telemetry values ring shared-memory object name.
 *
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_telemetry_values_shared_unlink(const char *name);

#endif // RTSYN_SPSC_TELEMETRY_VALUES_H
