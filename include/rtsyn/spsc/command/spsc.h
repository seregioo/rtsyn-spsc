/**
 * @file spsc_command.h
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Header file for the SPSC Command
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */
#ifndef rtsyn_COMMAND_SPSC_H
#define rtsyn_COMMAND_SPSC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rtsyn/spsc/command/message.h"
#include "rtsyn/spsc/common.h"

#define RTSYN_SPSC_COMMAND_CAPACITY 1024

#define SPSC_COMMAND_CAPACITY       RTSYN_SPSC_COMMAND_CAPACITY

/**
 * @brief Fixed-capacity command SPSC queue stored directly in shared memory.
 *
 * One producer process may call @ref rtsyn_spsc_command_try_push and one
 * consumer process may call @ref rtsyn_spsc_command_try_pop. The hot path does
 * not allocate, lock, or perform syscalls after the shared memory has been
 * mapped.
 */
typedef struct rtsyn_spsc_command_queue_e {
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE) uint64_t head;
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE) uint64_t tail;
    RTSYN_SPSC_ALIGNAS(RTSYN_SPSC_CACHE_LINE_SIZE)
    rtsyn_spsc_command_message_t buffer[RTSYN_SPSC_COMMAND_CAPACITY];
} rtsyn_spsc_command_queue_t;

/**
 * @brief Process-local handle for a mapped command queue.
 *
 * The handle itself is not shared; only @ref queue points into shared memory.
 */
typedef struct rtsyn_spsc_command_shared_e {
    rtsyn_spsc_command_queue_t *queue;
    int fd;
    char name[RTSYN_SPSC_SHM_NAME_MAX];
} rtsyn_spsc_command_shared_t;

RTSYN_SPSC_STATIC_ASSERT((RTSYN_SPSC_COMMAND_CAPACITY & (RTSYN_SPSC_COMMAND_CAPACITY - 1)) == 0,
                         "RTSYN_SPSC_COMMAND_CAPACITY must be a power of two");
RTSYN_SPSC_STATIC_ASSERT(RTSYN_SPSC_ATOMIC_U64_ALWAYS_LOCK_FREE,
                         "uint64_t atomics must be always lock-free for realtime SPSC cursors");
RTSYN_SPSC_STATIC_ASSERT(RTSYN_SPSC_ALIGNOF(rtsyn_spsc_command_queue_t)
                             == RTSYN_SPSC_CACHE_LINE_SIZE,
                         "command queue alignment ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_command_queue_t, head) == 0,
                         "command queue head offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_command_queue_t, tail)
                             == RTSYN_SPSC_CACHE_LINE_SIZE,
                         "command queue tail offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_command_queue_t, buffer)
                             == RTSYN_SPSC_CACHE_LINE_SIZE * 2,
                         "command queue buffer offset ABI changed");

/**
 * @brief Initialize an empty command queue.
 *
 * @param q Command queue to initialize. Passing NULL is allowed and has no
 * effect.
 */
void
rtsyn_spsc_command_init(rtsyn_spsc_command_queue_t *q);

/**
 * @brief Try to append one command message.
 *
 * @param q Command queue owned by exactly one producer process.
 * @param msg Command message to copy into the queue.
 * @return true when the message was written, false when arguments are invalid
 * or the queue is full.
 *
 * @note A false result is non-blocking backpressure. The owner of the command
 * SPSC must decide the policy: create/provision a larger or separate queue,
 * retry from a non-realtime context, or stall execution if the command is
 * mandatory.
 */
bool
rtsyn_spsc_command_try_push(rtsyn_spsc_command_queue_t *q, const rtsyn_spsc_command_message_t *msg);

/**
 * @brief Try to read one command message.
 *
 * @param q Command queue owned by exactly one consumer process.
 * @param msg Output message destination.
 * @return true when a message was read, false when arguments are invalid or the
 * queue is empty.
 */
bool
rtsyn_spsc_command_try_pop(rtsyn_spsc_command_queue_t *q, rtsyn_spsc_command_message_t *msg);

/**
 * @brief Return the fixed command queue capacity.
 *
 * @return Maximum number of queued command messages.
 */
size_t
rtsyn_spsc_command_capacity(void);

/**
 * @brief Return the current command queue occupancy.
 *
 * @param q Command queue to inspect.
 * @return Number of queued command messages, or 0 when @p q is NULL.
 */
size_t
rtsyn_spsc_command_size(const rtsyn_spsc_command_queue_t *q);

/**
 * @brief Create and map a command queue shared-memory object.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_command_shared_create(rtsyn_spsc_command_shared_t *shared, const char *name);

/**
 * @brief Create, map, prefault, and lock a command queue shared-memory object.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 *
 * This setup API may call syscalls and can fail if the process lacks enough
 * locked-memory allowance. It is intended to run before entering realtime code.
 */
int
rtsyn_spsc_command_shared_create_locked(rtsyn_spsc_command_shared_t *shared, const char *name);

/**
 * @brief Open and map an existing command queue shared-memory object.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_command_shared_open(rtsyn_spsc_command_shared_t *shared, const char *name);

/**
 * @brief Open, map, prefault, and lock an existing command queue.
 *
 * @param shared Process-local output handle to initialize.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 *
 * This setup API may call syscalls and can fail if the process lacks enough
 * locked-memory allowance. It is intended to run before entering realtime code.
 */
int
rtsyn_spsc_command_shared_open_locked(rtsyn_spsc_command_shared_t *shared, const char *name);

/**
 * @brief Unmap and close a process-local command queue handle.
 *
 * @param shared Process-local handle to close. Passing NULL is allowed and has
 * no effect.
 */
void
rtsyn_spsc_command_shared_close(rtsyn_spsc_command_shared_t *shared);

/**
 * @brief Remove a command queue shared-memory object name.
 *
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_command_shared_unlink(const char *name);

#endif // rtsyn_COMMAND_SPSC_H
