/**
 * @file queue.c
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Internal abstract SPSC queue implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "rtsyn/internal/spsc/queue.h"
#include "rtsyn/spsc/common.h"
#include "rtsyn/spsc/defaults.h"

/**
 * @brief Read one byte from each page to fault an existing mapping into memory.
 *
 * @param mapping Start of the mapped shared-memory region.
 * @param mapping_size Size of the mapped region in bytes.
 */
static void
rtsyn_spsc_prefault_mapping_read(void *mapping, size_t mapping_size);

/**
 * @brief Validate a POSIX shared-memory object name.
 *
 * @param name Candidate shared-memory name.
 * @return true when @p name is non-NULL, starts with '/', and fits in the
 * stored name buffer.
 */
static bool
rtsyn_spsc_name_is_valid(const char *name);

/**
 * @brief Map a shared-memory file descriptor and optionally prefault/lock it.
 *
 * @param queue Output mapped queue pointer.
 * @param fd Shared-memory file descriptor.
 * @param stored_name Output storage for the accepted shared-memory name.
 * @param stored_name_capacity Capacity of @p stored_name in bytes.
 * @param name Accepted shared-memory name to store in @p stored_name.
 * @param queue_size Mapping size in bytes.
 * @param lock_memory Whether to prefault and mlock the mapping.
 * @param initialize_mapping Whether this call owns initialization of a newly
 * created mapping and may dirty all pages.
 * @return 0 on success, -1 on failure with errno set.
 */
static int
rtsyn_spsc_shared_map(void **queue, int fd, char *stored_name, size_t stored_name_capacity,
                      const char *name, size_t queue_size, bool lock_memory,
                      bool initialize_mapping);

/**
 * @brief Shared implementation for creating queue shared memory.
 *
 * @param queue Output mapped queue pointer.
 * @param fd Output shared-memory file descriptor.
 * @param stored_name Output storage for the accepted shared-memory name.
 * @param stored_name_capacity Capacity of @p stored_name in bytes.
 * @param name POSIX shared-memory name.
 * @param queue_size Mapping size in bytes.
 * @param lock_memory Whether to prefault and mlock the mapping.
 * @return 0 on success, -1 on failure with errno set.
 */
static int
rtsyn_spsc_shared_create_impl(void **queue, int *fd, char *stored_name,
                              size_t stored_name_capacity, const char *name, size_t queue_size,
                              bool lock_memory);

/**
 * @brief Shared implementation for opening queue shared memory.
 *
 * @param queue Output mapped queue pointer.
 * @param fd Output shared-memory file descriptor.
 * @param stored_name Output storage for the accepted shared-memory name.
 * @param stored_name_capacity Capacity of @p stored_name in bytes.
 * @param name POSIX shared-memory name.
 * @param queue_size Mapping size in bytes.
 * @param lock_memory Whether to prefault and mlock the mapping.
 * @return 0 on success, -1 on failure with errno set.
 */
static int
rtsyn_spsc_shared_open_impl(void **queue, int *fd, char *stored_name, size_t stored_name_capacity,
                            const char *name, size_t queue_size, bool lock_memory);

static void
rtsyn_spsc_prefault_mapping_read(void *mapping, size_t mapping_size)
{
    volatile unsigned char *bytes = mapping;
    volatile unsigned char sink = 0;
    long page_size = sysconf(_SC_PAGESIZE);

    if (page_size <= 0)
    {
        page_size = RTSYN_SPSC_FALLBACK_PAGE_SIZE;
    }

    for (size_t offset = 0; offset < mapping_size; offset += (size_t)page_size)
    {
        sink = (unsigned char)(sink ^ bytes[offset]);
    }

    sink = (unsigned char)(sink ^ bytes[mapping_size - 1]);
}

static bool
rtsyn_spsc_name_is_valid(const char *name)
{
    return name != NULL && name[0] == '/'
           && strnlen(name, RTSYN_SPSC_SHM_NAME_MAX) < RTSYN_SPSC_SHM_NAME_MAX;
}

static int
rtsyn_spsc_shared_map(void **queue, int fd, char *stored_name, size_t stored_name_capacity,
                      const char *name, size_t queue_size, bool lock_memory,
                      bool initialize_mapping)
{
    void *mapped_queue = mmap(NULL, queue_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_queue == MAP_FAILED)
    {
        return -1;
    }

    if (lock_memory)
    {
        if (initialize_mapping)
        {
            memset(mapped_queue, 0, queue_size);
        }
        else
        {
            rtsyn_spsc_prefault_mapping_read(mapped_queue, queue_size);
        }

        if (mlock(mapped_queue, queue_size) == -1)
        {
            int saved_errno = errno;
            munmap(mapped_queue, queue_size);
            errno = saved_errno;
            return -1;
        }
    }

    *queue = mapped_queue;
    strncpy(stored_name, name, stored_name_capacity - 1);
    stored_name[stored_name_capacity - 1] = '\0';
    return 0;
}

static int
rtsyn_spsc_shared_create_impl(void **queue, int *fd, char *stored_name,
                              size_t stored_name_capacity, const char *name, size_t queue_size,
                              bool lock_memory)
{
    if (queue == NULL || fd == NULL || stored_name == NULL || stored_name_capacity == 0
        || !rtsyn_spsc_name_is_valid(name) || queue_size == 0)
    {
        errno = EINVAL;
        return -1;
    }

    *queue = NULL;
    *fd = -1;
    stored_name[0] = '\0';

    int created_fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (created_fd == -1)
    {
        return -1;
    }

    if (ftruncate(created_fd, (off_t)queue_size) == -1)
    {
        int saved_errno = errno;
        close(created_fd);
        shm_unlink(name);
        errno = saved_errno;
        return -1;
    }

    if (rtsyn_spsc_shared_map(queue, created_fd, stored_name, stored_name_capacity, name,
                              queue_size, lock_memory, true)
        == -1)
    {
        int saved_errno = errno;
        close(created_fd);
        shm_unlink(name);
        errno = saved_errno;
        return -1;
    }

    *fd = created_fd;
    return 0;
}

int
rtsyn_spsc_shared_create(void **queue, int *fd, char *stored_name, size_t stored_name_capacity,
                         const char *name, size_t queue_size)
{
    return rtsyn_spsc_shared_create_impl(queue, fd, stored_name, stored_name_capacity, name,
                                         queue_size, false);
}

int
rtsyn_spsc_shared_create_locked(void **queue, int *fd, char *stored_name,
                                size_t stored_name_capacity, const char *name, size_t queue_size)
{
    return rtsyn_spsc_shared_create_impl(queue, fd, stored_name, stored_name_capacity, name,
                                         queue_size, true);
}

static int
rtsyn_spsc_shared_open_impl(void **queue, int *fd, char *stored_name, size_t stored_name_capacity,
                            const char *name, size_t queue_size, bool lock_memory)
{
    if (queue == NULL || fd == NULL || stored_name == NULL || stored_name_capacity == 0
        || !rtsyn_spsc_name_is_valid(name) || queue_size == 0)
    {
        errno = EINVAL;
        return -1;
    }

    *queue = NULL;
    *fd = -1;
    stored_name[0] = '\0';

    int opened_fd = shm_open(name, O_RDWR, 0600);
    if (opened_fd == -1)
    {
        return -1;
    }

    if (rtsyn_spsc_shared_map(queue, opened_fd, stored_name, stored_name_capacity, name, queue_size,
                              lock_memory, false)
        == -1)
    {
        int saved_errno = errno;
        close(opened_fd);
        errno = saved_errno;
        return -1;
    }

    *fd = opened_fd;
    return 0;
}

int
rtsyn_spsc_shared_open(void **queue, int *fd, char *stored_name, size_t stored_name_capacity,
                       const char *name, size_t queue_size)
{
    return rtsyn_spsc_shared_open_impl(queue, fd, stored_name, stored_name_capacity, name,
                                       queue_size, false);
}

int
rtsyn_spsc_shared_open_locked(void **queue, int *fd, char *stored_name,
                              size_t stored_name_capacity, const char *name, size_t queue_size)
{
    return rtsyn_spsc_shared_open_impl(queue, fd, stored_name, stored_name_capacity, name,
                                       queue_size, true);
}

void
rtsyn_spsc_shared_close(void *queue, int fd, size_t queue_size)
{
    if (queue != NULL && queue_size != 0)
    {
        munmap(queue, queue_size);
    }

    if (fd != -1)
    {
        close(fd);
    }
}

int
rtsyn_spsc_shared_unlink(const char *name)
{
    if (!rtsyn_spsc_name_is_valid(name))
    {
        errno = EINVAL;
        return -1;
    }

    return shm_unlink(name);
}
