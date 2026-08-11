/**
 * @file queue.h
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Internal abstract SPSC queue implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */
#ifndef RTSYN_INTERNAL_SPSC_QUEUE_H
#define RTSYN_INTERNAL_SPSC_QUEUE_H

#include <stddef.h>

/**
 * @brief Create and map a POSIX shared-memory region for a typed SPSC queue.
 *
 * @param queue Output pointer that receives the mapped region address.
 * @param fd Output file descriptor for the shared-memory object.
 * @param stored_name Output buffer that stores the accepted shared-memory name.
 * @param stored_name_capacity Capacity of @p stored_name in bytes.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @param queue_size Size of the typed queue region in bytes.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_shared_create(void **queue, int *fd, char *stored_name, size_t stored_name_capacity,
                         const char *name, size_t queue_size);

/**
 * @brief Create, map, dirty-prefault, and lock a shared-memory queue region.
 *
 * This variant is intended for setup before realtime execution. Because it
 * creates the shared-memory object with exclusive ownership, it may write-touch
 * the whole mapping before locking it.
 *
 * @param queue Output pointer that receives the mapped region address.
 * @param fd Output file descriptor for the shared-memory object.
 * @param stored_name Output buffer that stores the accepted shared-memory name.
 * @param stored_name_capacity Capacity of @p stored_name in bytes.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @param queue_size Size of the typed queue region in bytes.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_shared_create_locked(void **queue, int *fd, char *stored_name,
                                size_t stored_name_capacity, const char *name, size_t queue_size);

/**
 * @brief Open and map an existing POSIX shared-memory SPSC queue region.
 *
 * @param queue Output pointer that receives the mapped region address.
 * @param fd Output file descriptor for the shared-memory object.
 * @param stored_name Output buffer that stores the accepted shared-memory name.
 * @param stored_name_capacity Capacity of @p stored_name in bytes.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @param queue_size Size of the typed queue region in bytes.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_shared_open(void **queue, int *fd, char *stored_name, size_t stored_name_capacity,
                       const char *name, size_t queue_size);

/**
 * @brief Open, map, read-prefault, and lock an existing shared-memory queue region.
 *
 * This variant is intended for setup before realtime execution. It only reads
 * from the existing mapping before locking it, so opening a live queue does not
 * dirty or overwrite producer/consumer data.
 *
 * @param queue Output pointer that receives the mapped region address.
 * @param fd Output file descriptor for the shared-memory object.
 * @param stored_name Output buffer that stores the accepted shared-memory name.
 * @param stored_name_capacity Capacity of @p stored_name in bytes.
 * @param name POSIX shared-memory name. It must start with '/'.
 * @param queue_size Size of the typed queue region in bytes.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_shared_open_locked(void **queue, int *fd, char *stored_name,
                              size_t stored_name_capacity, const char *name, size_t queue_size);

/**
 * @brief Unmap and close a mapped shared-memory queue region.
 *
 * @param queue Mapped region address. Passing NULL is allowed.
 * @param fd Shared-memory file descriptor. Passing -1 is allowed.
 * @param queue_size Size of the mapped queue region in bytes.
 */
void
rtsyn_spsc_shared_close(void *queue, int fd, size_t queue_size);

/**
 * @brief Remove a POSIX shared-memory object name.
 *
 * @param name POSIX shared-memory name. It must start with '/'.
 * @return 0 on success, -1 on failure with errno set.
 */
int
rtsyn_spsc_shared_unlink(const char *name);

#endif // RTSYN_INTERNAL_SPSC_QUEUE_H
