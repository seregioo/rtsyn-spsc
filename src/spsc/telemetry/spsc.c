/**
 * @file spsc_telemetry.c
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Compilation unit for the SPSC Command
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */

#include <errno.h>

#include "rtsyn/internal/spsc/queue.h"
#include "rtsyn/spsc/telemetry/spsc.h"

/**
 * @brief Shared implementation for creating telemetry event queue mappings.
 *
 * @param shared Process-local telemetry event queue handle to initialize.
 * @param name POSIX shared-memory name.
 * @param lock_memory Whether to prefault and mlock the mapping.
 * @return 0 on success, -1 on failure with errno set.
 */
static int
rtsyn_spsc_telemetry_shared_create_impl(rtsyn_spsc_telemetry_shared_t *shared, const char *name,
                                        bool lock_memory);

/**
 * @brief Shared implementation for opening telemetry event queue mappings.
 *
 * @param shared Process-local telemetry event queue handle to initialize.
 * @param name POSIX shared-memory name.
 * @param lock_memory Whether to prefault and mlock the mapping.
 * @return 0 on success, -1 on failure with errno set.
 */
static int
rtsyn_spsc_telemetry_shared_open_impl(rtsyn_spsc_telemetry_shared_t *shared, const char *name,
                                      bool lock_memory);

void
rtsyn_spsc_telemetry_init(rtsyn_spsc_telemetry_queue_t *q)
{
    if (q == NULL)
    {
        return;
    }

    __atomic_store_n(&q->head, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&q->tail, 0, __ATOMIC_RELAXED);
}

bool
rtsyn_spsc_telemetry_try_push(rtsyn_spsc_telemetry_queue_t *q,
                              const rtsyn_spsc_telemetry_message_t *msg)
{
    if (q == NULL || msg == NULL)
    {
        return false;
    }

    uint64_t head = __atomic_load_n(&q->head, __ATOMIC_RELAXED);
    uint64_t tail = __atomic_load_n(&q->tail, __ATOMIC_ACQUIRE);

    if ((head - tail) >= RTSYN_SPSC_TELEMETRY_CAPACITY)
    {
        return false;
    }

    q->buffer[head & (RTSYN_SPSC_TELEMETRY_CAPACITY - 1)] = *msg;

    __atomic_store_n(&q->head, head + 1, __ATOMIC_RELEASE);
    return true;
}

bool
rtsyn_spsc_telemetry_try_pop(rtsyn_spsc_telemetry_queue_t *q, rtsyn_spsc_telemetry_message_t *msg)
{
    if (q == NULL || msg == NULL)
    {
        return false;
    }

    uint64_t tail = __atomic_load_n(&q->tail, __ATOMIC_RELAXED);
    uint64_t head = __atomic_load_n(&q->head, __ATOMIC_ACQUIRE);

    if (tail == head)
    {
        return false;
    }

    *msg = q->buffer[tail & (RTSYN_SPSC_TELEMETRY_CAPACITY - 1)];

    __atomic_store_n(&q->tail, tail + 1, __ATOMIC_RELEASE);
    return true;
}

bool
rtsyn_spsc_telemetry_try_publish_values(rtsyn_spsc_telemetry_queue_t *q,
                                        rtsyn_spsc_telemetry_values_t *values,
                                        rtsyn_spsc_telemetry_message_t *msg,
                                        const rtsyn_spsc_telemetry_value_t *samples,
                                        size_t sample_count)
{
    if (q == NULL || values == NULL || msg == NULL || samples == NULL || sample_count == 0
        || sample_count > UINT32_MAX)
    {
        return false;
    }

    uint64_t event_head = __atomic_load_n(&q->head, __ATOMIC_RELAXED);
    uint64_t event_tail = __atomic_load_n(&q->tail, __ATOMIC_ACQUIRE);
    if ((event_head - event_tail) >= RTSYN_SPSC_TELEMETRY_CAPACITY)
    {
        return false;
    }

    uint64_t values_start_index = 0;
    if (!rtsyn_spsc_telemetry_values_try_push(values, samples, sample_count, &values_start_index))
    {
        return false;
    }

    msg->type = RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_VALUES_WRITTEN;
    msg->data.values_written.values_start_index = values_start_index;
    msg->data.values_written.value_count = (uint32_t)sample_count;

    return rtsyn_spsc_telemetry_try_push(q, msg);
}

size_t
rtsyn_spsc_telemetry_capacity(void)
{
    return RTSYN_SPSC_TELEMETRY_CAPACITY;
}

size_t
rtsyn_spsc_telemetry_size(const rtsyn_spsc_telemetry_queue_t *q)
{
    if (q == NULL)
    {
        return 0;
    }

    uint64_t head = __atomic_load_n(&q->head, __ATOMIC_ACQUIRE);
    uint64_t tail = __atomic_load_n(&q->tail, __ATOMIC_ACQUIRE);
    return (size_t)(head - tail);
}

static int
rtsyn_spsc_telemetry_shared_create_impl(rtsyn_spsc_telemetry_shared_t *shared, const char *name,
                                        bool lock_memory)
{
    if (shared == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    void *queue = NULL;
    int result =
        lock_memory
            ? rtsyn_spsc_shared_create_locked(&queue, &shared->fd, shared->name,
                                              sizeof(shared->name), name,
                                              sizeof(rtsyn_spsc_telemetry_queue_t))
            : rtsyn_spsc_shared_create(&queue, &shared->fd, shared->name, sizeof(shared->name),
                                       name, sizeof(rtsyn_spsc_telemetry_queue_t));
    shared->queue = queue;
    if (result == 0)
    {
        rtsyn_spsc_telemetry_init(shared->queue);
    }

    return result;
}

int
rtsyn_spsc_telemetry_shared_create(rtsyn_spsc_telemetry_shared_t *shared, const char *name)
{
    return rtsyn_spsc_telemetry_shared_create_impl(shared, name, false);
}

int
rtsyn_spsc_telemetry_shared_create_locked(rtsyn_spsc_telemetry_shared_t *shared, const char *name)
{
    return rtsyn_spsc_telemetry_shared_create_impl(shared, name, true);
}

static int
rtsyn_spsc_telemetry_shared_open_impl(rtsyn_spsc_telemetry_shared_t *shared, const char *name,
                                      bool lock_memory)
{
    if (shared == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    void *queue = NULL;
    int result =
        lock_memory
            ? rtsyn_spsc_shared_open_locked(&queue, &shared->fd, shared->name,
                                            sizeof(shared->name), name,
                                            sizeof(rtsyn_spsc_telemetry_queue_t))
            : rtsyn_spsc_shared_open(&queue, &shared->fd, shared->name, sizeof(shared->name),
                                     name, sizeof(rtsyn_spsc_telemetry_queue_t));
    shared->queue = queue;
    return result;
}

int
rtsyn_spsc_telemetry_shared_open(rtsyn_spsc_telemetry_shared_t *shared, const char *name)
{
    return rtsyn_spsc_telemetry_shared_open_impl(shared, name, false);
}

int
rtsyn_spsc_telemetry_shared_open_locked(rtsyn_spsc_telemetry_shared_t *shared, const char *name)
{
    return rtsyn_spsc_telemetry_shared_open_impl(shared, name, true);
}

void
rtsyn_spsc_telemetry_shared_close(rtsyn_spsc_telemetry_shared_t *shared)
{
    if (shared == NULL)
    {
        return;
    }

    rtsyn_spsc_shared_close(shared->queue, shared->fd, sizeof(rtsyn_spsc_telemetry_queue_t));
    shared->queue = NULL;
    shared->fd = -1;
    shared->name[0] = '\0';
}

int
rtsyn_spsc_telemetry_shared_unlink(const char *name)
{
    return rtsyn_spsc_shared_unlink(name);
}
