/**
 * @file command_message.h
 * @author Sergio Hidalgo (sergiohg.dev@gmail.com)
 * @brief Header file for the command messgae
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * @copyright Copyright (c) Sergio Hidalgo 2026
 */
#ifndef RTSYN_SPSC_COMMAND_MESSAGE_H
#define RTSYN_SPSC_COMMAND_MESSAGE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include <rtsyn/abi/node.h>
#include <rtsyn/abi/value.h>

#include "rtsyn/spsc/common.h"

#define RTSYN_SPSC_COMMAND_PARAM_STRING_MAX_SIZE 16
#define RTSYN_SPSC_COMMAND_NODE_NAME_MAX_SIZE    64
#define RTSYN_SPSC_COMMAND_MODULE_PATH_MAX_SIZE  256

/**
 * @brief Command message discriminator.
 *
 * @note This enum is stored inside the shared-memory IPC payload. Every process
 * using the queue must be built with a compatible C ABI for enum layout.
 */
typedef enum rtsyn_spsc_command_message_type_e {
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_NONE = 0,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_PLUGIN_UPDATE,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_REQUEST_PORT_VALUES,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_REQUEST_PORT_VARIABLES,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_LOAD_NODE,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_ADD_NODE,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_ADD_CONNECTION,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_REMOVE_CONNECTION,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_SET_PARAM,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_GLOBAL_COMMAND,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_SET_RUNTIME_PERIOD,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_SET_RUNTIME_PRIORITY,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_SET_RUNTIME_DEADLINE_TOLERANCE,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_REQUEST_RUNTIME_NODES,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_REMOVE_NODE,
    RTSYN_SPSC_COMMAND_MESSAGE_TYPE_MAX,
} rtsyn_spsc_command_message_type_t;

/**
 * @brief Request a plugin state update.
 */
typedef struct rtsyn_spsc_command_message_plugin_update_e {
    uint32_t plugin_id;
    uint8_t plugin_state;
} rtsyn_spsc_command_message_plugin_update_t;

/**
 * @brief Enable or disable telemetry for a plugin port-value mask.
 *
 * @note The @ref send field is part of the shared-memory IPC payload. C
 * stdbool must be used by all C/C++ participants, or a 1 byte size value if
 * stdbool is not available in another language binding.
 */
typedef struct rtsyn_spsc_command_message_plugin_request_port_values_e {
    uint32_t plugin_id;
    bool send;
    uint64_t portsyn_mask;
} rtsyn_spsc_command_message_plugin_request_port_values_t;

/**
 * @brief Enable or disable telemetry for a plugin variable mask.
 *
 * @note The @ref send field is part of the shared-memory IPC payload. C
 * stdbool must be used by all C/C++ participants, or a 1 byte size value if
 * stdbool is not available in another language binding.
 */
typedef struct rtsyn_spsc_command_message_plugin_request_variables_e {
    uint32_t plugin_id;
    bool send;
    uint64_t variable_mask;
} rtsyn_spsc_command_message_plugin_request_variables_t;

/**
 * @brief Load or replace one node module in the realtime process.
 */
typedef struct rtsyn_spsc_command_message_load_node_e {
    rtsyn_abi_node_type_t node_type;
    char module_path[RTSYN_SPSC_COMMAND_MODULE_PATH_MAX_SIZE];
} rtsyn_spsc_command_message_load_node_t;

/**
 * @brief Instantiate one runtime node from a previously loaded descriptor name.
 */
typedef struct rtsyn_spsc_command_message_add_node_e {
    rtsyn_abi_node_type_t node_type;
    char node_name[RTSYN_SPSC_COMMAND_NODE_NAME_MAX_SIZE];
} rtsyn_spsc_command_message_add_node_t;

/**
 * @brief Remove one runtime node by identifier.
 */
typedef struct rtsyn_spsc_command_message_remove_node_e {
    uint32_t node_id;
} rtsyn_spsc_command_message_remove_node_t;

/**
 * @brief Add a runtime connection by node and port identifiers.
 */
typedef struct rtsyn_spsc_command_message_add_connection_e {
    uint32_t connection_id;
    uint32_t source_node_id;
    uint32_t source_port_id;
    uint32_t destination_node_id;
    uint32_t destination_port_id;
} rtsyn_spsc_command_message_add_connection_t;

/**
 * @brief Remove a runtime connection by identifier.
 */
typedef struct rtsyn_spsc_command_message_remove_connection_e {
    uint32_t connection_id;
} rtsyn_spsc_command_message_remove_connection_t;

typedef union rtsyn_spsc_command_param_value_e {
    float f32;
    double f64;
    int64_t i64;
    uint64_t u64;
    char string[RTSYN_SPSC_COMMAND_PARAM_STRING_MAX_SIZE];
} rtsyn_spsc_command_param_value_t;

/**
 * @brief Set one parameter value on a runtime node.
 */
typedef struct rtsyn_spsc_command_message_set_param_e {
    uint32_t node_id;
    uint32_t param_id;
    rtsyn_abi_value_type_t value_type;
    rtsyn_spsc_command_param_value_t value;
} rtsyn_spsc_command_message_set_param_t;

/**
 * @brief Set the runtime cycle period.
 */
typedef struct rtsyn_spsc_command_message_set_runtime_period_e {
    uint64_t period_ns;
} rtsyn_spsc_command_message_set_runtime_period_t;

/**
 * @brief Set the realtime engine thread priority.
 */
typedef struct rtsyn_spsc_command_message_set_runtime_priority_e {
    int32_t priority;
} rtsyn_spsc_command_message_set_runtime_priority_t;

/**
 * @brief Set the runtime deadline tolerance used by measurement metrics.
 */
typedef struct rtsyn_spsc_command_message_set_runtime_deadline_tolerance_e {
    uint64_t tolerance_ns;
} rtsyn_spsc_command_message_set_runtime_deadline_tolerance_t;

/**
 * @brief Request a global command for the realtime process.
 */
typedef struct rtsyn_spsc_command_message_global_command_update_e {
    uint8_t command;
} rtsyn_spsc_command_message_global_command_update_t;

/**
 * @brief Fixed-size command IPC payload carried by the command SPSC queue.
 *
 * Commands are produced by the controller process and consumed by the realtime
 * process. The payload is copied by value into shared memory; do not place
 * pointers, process-local addresses, heap ownership, or variable-length data in
 * this structure.
 *
 * @note This structure contains C enums and C stdbool values through nested
 * payloads. All C/C++ processes must use compatible enum layout and C stdbool;
 * non-C bindings must represent stdbool fields with a 1 byte size value if
 * stdbool is not available.
 */
typedef struct rtsyn_command_message_e {
    uint64_t seq;
    uint64_t timestamp_ns;
    rtsyn_spsc_command_message_type_t type;

    union {
        rtsyn_spsc_command_message_plugin_update_t plugin_update;
        rtsyn_spsc_command_message_plugin_request_port_values_t plugin_request_ports;
        rtsyn_spsc_command_message_plugin_request_variables_t plugin_request_variables;
        rtsyn_spsc_command_message_load_node_t load_node;
        rtsyn_spsc_command_message_add_node_t add_node;
        rtsyn_spsc_command_message_remove_node_t remove_node;
        rtsyn_spsc_command_message_add_connection_t add_connection;
        rtsyn_spsc_command_message_remove_connection_t remove_connection;
        rtsyn_spsc_command_message_set_param_t set_param;
        rtsyn_spsc_command_message_set_runtime_period_t set_runtime_period;
        rtsyn_spsc_command_message_set_runtime_priority_t set_runtime_priority;
        rtsyn_spsc_command_message_set_runtime_deadline_tolerance_t
            set_runtime_deadline_tolerance;
        rtsyn_spsc_command_message_global_command_update_t global_command;
    } data;
} rtsyn_spsc_command_message_t;

RTSYN_SPSC_STATIC_ASSERT(sizeof(bool) == 1, "bool must be 1 byte for command IPC ABI");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_type_t) == sizeof(uint32_t),
                         "command message enum must be 4 bytes for IPC ABI");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_plugin_update_t) == 8,
                         "plugin update command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_plugin_request_port_values_t) == 16,
                         "plugin port request command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_plugin_request_variables_t) == 16,
                         "plugin variable request command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_load_node_t) == 260,
                         "load node command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_add_node_t) == 68,
                         "add node command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_remove_node_t) == 4,
                         "remove node command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_add_connection_t) == 20,
                         "add connection command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_remove_connection_t) == 4,
                         "remove connection command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_param_value_t) == 16,
                         "set-param command value payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_set_param_t) == 32,
                         "set-param command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_set_runtime_period_t) == 8,
                         "set-runtime-period command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_set_runtime_priority_t) == 4,
                         "set-runtime-priority command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(
    sizeof(rtsyn_spsc_command_message_set_runtime_deadline_tolerance_t) == 8,
    "set-runtime-deadline-tolerance command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_global_command_update_t) == 1,
                         "global command payload ABI changed");
RTSYN_SPSC_STATIC_ASSERT(sizeof(rtsyn_spsc_command_message_t) == 288,
                         "command message IPC ABI changed");
RTSYN_SPSC_STATIC_ASSERT(RTSYN_SPSC_ALIGNOF(rtsyn_spsc_command_message_t) == 8,
                         "command message alignment ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_command_message_t, seq) == 0,
                         "command message seq offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_command_message_t, timestamp_ns) == 8,
                         "command message timestamp offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_command_message_t, type) == 16,
                         "command message type offset ABI changed");
RTSYN_SPSC_STATIC_ASSERT(offsetof(rtsyn_spsc_command_message_t, data) == 24,
                         "command message data offset ABI changed");

#endif // RTSYN_SPSC_COMMAND_MESSAGE_H
