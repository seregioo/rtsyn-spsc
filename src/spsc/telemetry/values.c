/**
 * @file values.c
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Shared telemetry value ring implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */

#include <errno.h>

#include "rtsyn/internal/spsc/queue.h"
#include "rtsyn/spsc/telemetry/values.h"

/**
 * @brief Shared implementation for creating telemetry values ring mappings.
 *
 * @param shared Process-local telemetry values ring handle to initialize.
 * @param name POSIX shared-memory name.
 * @param lock_memory Whether to prefault and mlock the mapping.
 * @return 0 on success, -1 on failure with errno set.
 */
static int
rtsyn_spsc_telemetry_values_shared_create_impl(rtsyn_spsc_telemetry_values_shared_t *shared,
                                               const char *name, bool lock_memory);

/**
 * @brief Shared implementation for opening telemetry values ring mappings.
 *
 * @param shared Process-local telemetry values ring handle to initialize.
 * @param name POSIX shared-memory name.
 * @param lock_memory Whether to prefault and mlock the mapping.
 * @return 0 on success, -1 on failure with errno set.
 */
static int
rtsyn_spsc_telemetry_values_shared_open_impl(rtsyn_spsc_telemetry_values_shared_t *shared,
                                             const char *name, bool lock_memory);

void
rtsyn_spsc_telemetry_values_init(rtsyn_spsc_telemetry_values_t *values)
{
    if (values == NULL)
    {
        return;
    }

    __atomic_store_n(&values->head, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&values->tail, 0, __ATOMIC_RELAXED);
}

size_t
rtsyn_spsc_telemetry_values_capacity(void)
{
    return RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY;
}

size_t
rtsyn_spsc_telemetry_values_size(const rtsyn_spsc_telemetry_values_t *values)
{
    if (values == NULL)
    {
        return 0;
    }

    uint64_t head = __atomic_load_n(&values->head, __ATOMIC_ACQUIRE);
    uint64_t tail = __atomic_load_n(&values->tail, __ATOMIC_ACQUIRE);
    return (size_t)(head - tail);
}

size_t
rtsyn_spsc_telemetry_values_free_space(const rtsyn_spsc_telemetry_values_t *values)
{
    return RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY - rtsyn_spsc_telemetry_values_size(values);
}

bool
rtsyn_spsc_telemetry_values_try_push(rtsyn_spsc_telemetry_values_t *values,
                                     const rtsyn_spsc_telemetry_value_t *samples,
                                     size_t sample_count, uint64_t *start_index)
{
    if (values == NULL || (samples == NULL && sample_count != 0)
        || sample_count > RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY)
    {
        return false;
    }

    uint64_t head = __atomic_load_n(&values->head, __ATOMIC_RELAXED);
    uint64_t tail = __atomic_load_n(&values->tail, __ATOMIC_ACQUIRE);

    if ((head - tail) + sample_count > RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY)
    {
        return false;
    }

    if (start_index != NULL)
    {
        *start_index = head;
    }

    for (size_t i = 0; i < sample_count; ++i)
    {
        values->buffer[(head + i) & (RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY - 1)] = samples[i];
    }

    __atomic_store_n(&values->head, head + sample_count, __ATOMIC_RELEASE);
    return true;
}

bool
rtsyn_spsc_telemetry_values_try_get(const rtsyn_spsc_telemetry_values_t *values, uint64_t index,
                                    rtsyn_spsc_telemetry_value_t *sample)
{
    if (values == NULL || sample == NULL)
    {
        return false;
    }

    uint64_t head = __atomic_load_n(&values->head, __ATOMIC_ACQUIRE);
    uint64_t tail = __atomic_load_n(&values->tail, __ATOMIC_ACQUIRE);

    if (index < tail || index >= head)
    {
        return false;
    }

    *sample = values->buffer[index & (RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY - 1)];
    return true;
}

bool
rtsyn_spsc_telemetry_values_release(rtsyn_spsc_telemetry_values_t *values, size_t sample_count)
{
    if (values == NULL)
    {
        return false;
    }

    uint64_t tail = __atomic_load_n(&values->tail, __ATOMIC_RELAXED);
    uint64_t head = __atomic_load_n(&values->head, __ATOMIC_ACQUIRE);

    if (sample_count > head - tail)
    {
        return false;
    }

    __atomic_store_n(&values->tail, tail + sample_count, __ATOMIC_RELEASE);
    return true;
}

static int
rtsyn_spsc_telemetry_values_shared_create_impl(rtsyn_spsc_telemetry_values_shared_t *shared,
                                               const char *name, bool lock_memory)
{
    if (shared == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    void *values = NULL;
    int result =
        lock_memory
            ? rtsyn_spsc_shared_create_locked(&values, &shared->fd, shared->name,
                                              sizeof(shared->name), name,
                                              sizeof(rtsyn_spsc_telemetry_values_t))
            : rtsyn_spsc_shared_create(&values, &shared->fd, shared->name, sizeof(shared->name),
                                       name, sizeof(rtsyn_spsc_telemetry_values_t));
    shared->values = values;
    if (result == 0)
    {
        rtsyn_spsc_telemetry_values_init(shared->values);
    }

    return result;
}

int
rtsyn_spsc_telemetry_values_shared_create(rtsyn_spsc_telemetry_values_shared_t *shared,
                                          const char *name)
{
    return rtsyn_spsc_telemetry_values_shared_create_impl(shared, name, false);
}

int
rtsyn_spsc_telemetry_values_shared_create_locked(rtsyn_spsc_telemetry_values_shared_t *shared,
                                                 const char *name)
{
    return rtsyn_spsc_telemetry_values_shared_create_impl(shared, name, true);
}

static int
rtsyn_spsc_telemetry_values_shared_open_impl(rtsyn_spsc_telemetry_values_shared_t *shared,
                                             const char *name, bool lock_memory)
{
    if (shared == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    void *values = NULL;
    int result =
        lock_memory
            ? rtsyn_spsc_shared_open_locked(&values, &shared->fd, shared->name,
                                            sizeof(shared->name), name,
                                            sizeof(rtsyn_spsc_telemetry_values_t))
            : rtsyn_spsc_shared_open(&values, &shared->fd, shared->name, sizeof(shared->name),
                                     name, sizeof(rtsyn_spsc_telemetry_values_t));
    shared->values = values;
    return result;
}

int
rtsyn_spsc_telemetry_values_shared_open(rtsyn_spsc_telemetry_values_shared_t *shared,
                                        const char *name)
{
    return rtsyn_spsc_telemetry_values_shared_open_impl(shared, name, false);
}

int
rtsyn_spsc_telemetry_values_shared_open_locked(rtsyn_spsc_telemetry_values_shared_t *shared,
                                               const char *name)
{
    return rtsyn_spsc_telemetry_values_shared_open_impl(shared, name, true);
}

void
rtsyn_spsc_telemetry_values_shared_close(rtsyn_spsc_telemetry_values_shared_t *shared)
{
    if (shared == NULL)
    {
        return;
    }

    rtsyn_spsc_shared_close(shared->values, shared->fd, sizeof(rtsyn_spsc_telemetry_values_t));
    shared->values = NULL;
    shared->fd = -1;
    shared->name[0] = '\0';
}

int
rtsyn_spsc_telemetry_values_shared_unlink(const char *name)
{
    return rtsyn_spsc_shared_unlink(name);
}
