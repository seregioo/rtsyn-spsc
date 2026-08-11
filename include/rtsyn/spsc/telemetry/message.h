/**
 * @file telemetry_message.h
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Header file for the telemetry messgae
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */
#ifndef RTSYN_SPSC_TELEMETRY_MESSAGE_H
#define RTSYN_SPSC_TELEMETRY_MESSAGE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "rtsyn/spsc/common.h"
#include "rtsyn/spsc/telemetry/values.h"

/**
 * @brief Telemetry event discriminator.
 *
 * @note This enum is stored inside the shared-memory IPC payload. Every process
 * using the queue must be built with a compatible C ABI for enum layout.
 */
typedef enum rtsyn_spsc_telemetry_message_type_e {
    RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_NONE = 0,
    RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_CYCLE_BEGIN,
    RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_CYCLE_END,
    RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_NODE_STATUS,
    RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_VALUES_WRITTEN,
    RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_CIRCUIT_SNAPSHOT,
    RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_DROPPED,
    RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_MAX,
} rtsyn_spsc_telemetry_message_type_t;

/**
 * @brief Per-node execution status reported by the realtime process.
 *
 * @note This enum is stored inside the shared-memory IPC payload. Every process
 * using the queue must be built with a compatible C ABI for enum layout.
 */
typedef enum rtsyn_spsc_telemetry_node_status_e {
    RTSYN_SPSC_TELEMETRY_NODE_STATUS_NONE = 0,
    RTSYN_SPSC_TELEMETRY_NODE_STATUS_OK,
    RTSYN_SPSC_TELEMETRY_NODE_STATUS_ERROR,
    RTSYN_SPSC_TELEMETRY_NODE_STATUS_SKIPPED,
    RTSYN_SPSC_TELEMETRY_NODE_STATUS_MAX,
} rtsyn_spsc_telemetry_node_status_t;

/**
 * @brief Marks the beginning of a realtime cycle.
 */
typedef struct rtsyn_spsc_telemetry_message_cycle_begin_e {
    uint64_t cycle_id;
    uint64_t scheduled_timestamp_ns;
} rtsyn_spsc_telemetry_message_cycle_begin_t;

/**
 * @brief Summarizes a completed realtime cycle.
 */
typedef struct rtsyn_spsc_telemetry_message_cycle_end_e {
    uint64_t cycle_id;
    uint64_t started_at_ns;
    uint64_t finished_at_ns;
    uint32_t processed_device_count;
    uint32_t processed_plugin_count;
    uint32_t error_count;
    uint32_t overrun_count;
} rtsyn_spsc_telemetry_message_cycle_end_t;

/**
 * @brief Reports the execution result for one device or plugin node.
 */
typedef struct rtsyn_spsc_telemetry_message_node_status_e {
    uint64_t cycle_id;
    uint32_t node_id;
    rtsyn_spsc_telemetry_source_t source;
    rtsyn_spsc_telemetry_node_status_t status;
    uint32_t status_code;
    uint64_t started_at_ns;
    uint64_t finished_at_ns;
} rtsyn_spsc_telemetry_message_node_status_t;

/**
 * @brief References a contiguous value range written to the telemetry values ring.
 */
typedef struct rtsyn_spsc_telemetry_message_values_written_e {
    uint64_t cycle_id;
    uint32_t node_id;
    rtsyn_spsc_telemetry_source_t source;
    uint64_t values_start_index;
    uint32_t value_count;
} rtsyn_spsc_telemetry_message_values_written_t;

/**
 * @brief References an optional circuit snapshot stored in the values/blob stream.
 */
typedef struct rtsyn_spsc_telemetry_message_circuit_snapshot_e {
    uint64_t cycle_id;
    uint64_t circuit_revision;
    uint64_t values_start_index;
    uint32_t value_count;
} rtsyn_spsc_telemetry_message_circuit_snapshot_t;

/**
 * @brief Reports telemetry loss observed by the producer.
 *
 * This event is best-effort like any other telemetry event. If the telemetry
 * event queue is already full, the producer/owner must account for the loss
 * outside this queue.
 */
typedef struct rtsyn_spsc_telemetry_message_dropped_e {
    uint64_t cycle_id;
    uint64_t dropped_event_count;
    uint64_t dropped_value_count;
} rtsyn_spsc_telemetry_message_dropped_t;

/**
 * @brief Fixed-size telemetry event IPC payload.
 *
 * Telemetry is produced by the realtime process and consumed by one telemetry
 * process. Value-heavy events reference ranges in @ref rtsyn_spsc_telemetry_values_t
 * instead of embedding variable-length data in this event.
 *
 * @note This structure contains C enums through nested payloads. All C/C++
 * processes must use compatible enum layout. If another language binding maps
 * any future boolean field, C stdbool must be used, or a 1 byte size value if
 * stdbool is not available.
 */
typedef struct rtsyn_spsc_telemetry_message_e {
    uint64_t seq;
    uint64_t timestamp_ns;
    uint64_t dropped_event_count;
    rtsyn_spsc_telemetry_message_type_t type;

    union {
        rtsyn_spsc_telemetry_message_cycle_begin_t cycle_begin;
        rtsyn_spsc_telemetry_message_cycle_end_t cycle_end;
        rtsyn_spsc_telemetry_message_node_status_t node_status;
        rtsyn_spsc_telemetry_message_values_written_t values_written;
        rtsyn_spsc_telemetry_message_circuit_snapshot_t circuit_snapshot;
        rtsyn_spsc_telemetry_message_dropped_t dropped;
    } data;
} rtsyn_spsc_telemetry_message_t;

RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_telemetry_message_type_t) == sizeof(uint32_t),
                         "telemetry message enum must be 4 bytes for IPC ABI");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_telemetry_node_status_t) == sizeof(uint32_t),
                         "telemetry node status enum must be 4 bytes for IPC ABI");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_telemetry_message_cycle_begin_t) == 16,
                         "telemetry cycle begin payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_telemetry_message_cycle_end_t) == 40,
                         "telemetry cycle end payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_telemetry_message_node_status_t) == 40,
                         "telemetry node status payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_telemetry_message_values_written_t) == 32,
                         "telemetry values-written payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_telemetry_message_circuit_snapshot_t) == 32,
                         "telemetry circuit snapshot payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_telemetry_message_dropped_t) == 24,
                         "telemetry dropped payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_telemetry_message_t) == 72,
                         "telemetry message IPC ABI changed");
RTSYN_SPSC_STATIC_ASSERT(RTSYN_SPSC_ALIGNOF(rtsyn_spsc_telemetry_message_t) == 8,
                         "telemetry message alignment ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_message_t, seq) == 0,
                         "telemetry message seq offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_message_t, timestamp_ns) == 8,
                         "telemetry message timestamp offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_message_t, dropped_event_count) == 16,
                         "telemetry message dropped count offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_message_t, type) == 24,
                         "telemetry message type offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_telemetry_message_t, data) == 32,
                         "telemetry message data offset ABI changed");

#endif // RTSYN_SPSC_TELEMETRY_MESSAGE_H
