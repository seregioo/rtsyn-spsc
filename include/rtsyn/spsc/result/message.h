/**
 * @file result/message.h
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Command-result message payload for RTSyn SPSC IPC.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */
#ifndef RTSYN_SPSC_RESULT_MESSAGE_H
#define RTSYN_SPSC_RESULT_MESSAGE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <rtsyn/abi/node.h>
#include <rtsyn/abi/value.h>

#include "rtsyn/spsc/command/message.h"
#include "rtsyn/spsc/common.h"

#define RTSYN_SPSC_RESULT_NAME_MAX_SIZE        64
#define RTSYN_SPSC_RESULT_DESCRIPTION_MAX_SIZE 128
#define RTSYN_SPSC_RESULT_PORT_CAPACITY        16
#define RTSYN_SPSC_RESULT_PARAM_CAPACITY       16
#define RTSYN_SPSC_RESULT_STATE_CAPACITY       16

/**
 * @brief Command-result discriminator.
 */
typedef enum rtsyn_spsc_result_status_e {
    RTSYN_SPSC_RESULT_STATUS_NONE = 0,
    RTSYN_SPSC_RESULT_STATUS_OK,
    RTSYN_SPSC_RESULT_STATUS_ERROR,
    RTSYN_SPSC_RESULT_STATUS_MAX,
} rtsyn_spsc_result_status_t;

/**
 * @brief One port entry copied from a loaded node descriptor.
 */
typedef struct rtsyn_spsc_result_port_descriptor_e {
    uint32_t id;
    rtsyn_abi_value_type_t value_type;
    rtsyn_abi_port_direction_t direction;
    char name[RTSYN_SPSC_RESULT_NAME_MAX_SIZE];
} rtsyn_spsc_result_port_descriptor_t;

/**
 * @brief One parameter entry copied from a loaded node descriptor.
 */
typedef struct rtsyn_spsc_result_param_descriptor_e {
    uint32_t id;
    rtsyn_abi_value_type_t value_type;
    char name[RTSYN_SPSC_RESULT_NAME_MAX_SIZE];
    char description[RTSYN_SPSC_RESULT_DESCRIPTION_MAX_SIZE];
} rtsyn_spsc_result_param_descriptor_t;

/**
 * @brief One state entry copied from a loaded node descriptor.
 */
typedef struct rtsyn_spsc_result_state_descriptor_e {
    uint32_t id;
    rtsyn_abi_value_type_t value_type;
    char name[RTSYN_SPSC_RESULT_NAME_MAX_SIZE];
    char description[RTSYN_SPSC_RESULT_DESCRIPTION_MAX_SIZE];
} rtsyn_spsc_result_state_descriptor_t;

/**
 * @brief Fixed-size descriptor summary returned by the engine.
 */
typedef struct rtsyn_spsc_result_node_descriptor_e {
    rtsyn_abi_node_type_t node_type;
    uint32_t port_count;
    uint32_t param_count;
    uint32_t state_count;
    char name[RTSYN_SPSC_RESULT_NAME_MAX_SIZE];
    rtsyn_spsc_result_port_descriptor_t ports[RTSYN_SPSC_RESULT_PORT_CAPACITY];
    rtsyn_spsc_result_param_descriptor_t params[RTSYN_SPSC_RESULT_PARAM_CAPACITY];
    rtsyn_spsc_result_state_descriptor_t states[RTSYN_SPSC_RESULT_STATE_CAPACITY];
} rtsyn_spsc_result_node_descriptor_t;

/**
 * @brief Runtime connection snapshot returned by REQUEST_RUNTIME_NODES.
 */
typedef struct rtsyn_spsc_result_connection_descriptor_e {
    uint32_t connection_id;
    uint32_t source_node_id;
    uint32_t source_port_id;
    uint32_t destination_node_id;
    uint32_t destination_port_id;
    uint32_t timing;
} rtsyn_spsc_result_connection_descriptor_t;

/**
 * @brief Fixed-size command result IPC payload.
 */
typedef struct rtsyn_spsc_result_message_e {
    uint64_t seq;
    uint64_t timestamp_ns;
    rtsyn_spsc_command_message_type_t command_type;
    rtsyn_spsc_result_status_t status;
    uint32_t status_code;
    uint32_t node_id;
    rtsyn_spsc_result_node_descriptor_t node;
    rtsyn_spsc_result_connection_descriptor_t connection;
} rtsyn_spsc_result_message_t;

RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_result_status_t) == sizeof(uint32_t),
                         "result status enum must be 4 bytes for IPC ABI");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_result_port_descriptor_t) == 76,
                         "result port descriptor ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_result_param_descriptor_t) == 200,
                         "result param descriptor ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_result_state_descriptor_t) == 200,
                         "result state descriptor ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_result_connection_descriptor_t) == 24,
                         "result connection descriptor ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_result_message_t, seq) == 0,
                         "result message seq offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_result_message_t, timestamp_ns) == 8,
                         "result message timestamp offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_result_message_t, command_type) == 16,
                         "result message command type offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_result_message_t, status) == 20,
                         "result message status offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_result_message_t, connection)
                             > offsetof(rtsyn_spsc_result_message_t, node),
                         "result message connection offset ABI changed");

#endif // RTSYN_SPSC_RESULT_MESSAGE_H
