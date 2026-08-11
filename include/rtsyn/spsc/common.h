/**
 * @file common.h
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Common definitions for SPSC queues
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */
#ifndef RTSYN_SPSC_COMMON_H
#define RTSYN_SPSC_COMMON_H

#include <rtsyn/spsc/defaults.h>
#include <stdint.h>

#define RTSYN_SPSC_CACHE_LINE_SIZE 64

#if defined(__cplusplus)
#define RTSYN_SPSC_ALIGNAS(value)                    alignas(value)
#define RTSYN_SPSC_ALIGNOF(type)                     alignof(type)
#define RTSYN_SPSC_STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define RTSYN_SPSC_ALIGNAS(value)                    _Alignas(value)
#define RTSYN_SPSC_ALIGNOF(type)                     _Alignof(type)
#define RTSYN_SPSC_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif

#define RTSYN_SPSC_ATOMIC_U64_ALWAYS_LOCK_FREE __atomic_always_lock_free(sizeof(uint64_t), 0)

#endif // RTSYN_SPSC_COMMON_H
