#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern "C" {
#include "rtsyn/spsc/command/spsc.h"
}

namespace {

std::string
make_shm_name()
{
    return std::string("/rtsyn_spsc_command_") + std::to_string(getpid());
}

void
sleep_briefly()
{
    std::this_thread::sleep_for(std::chrono::microseconds(50));
}

void
child_exit(bool ok)
{
    std::fflush(nullptr);
    _exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
}

} // namespace

class SPSCCommandTest : public ::testing::Test {};

static_assert(sizeof(decltype(rtsyn_spsc_command_queue_t::head)) == sizeof(uint64_t));
static_assert(sizeof(decltype(rtsyn_spsc_command_queue_t::tail)) == sizeof(uint64_t));

TEST_F(SPSCCommandTest, RejectsNullArguments)
{
    rtsyn_spsc_command_queue_t queue;
    rtsyn_spsc_command_message_t message = {};

    rtsyn_spsc_command_init(&queue);

    EXPECT_FALSE(rtsyn_spsc_command_try_push(nullptr, &message));
    EXPECT_FALSE(rtsyn_spsc_command_try_push(&queue, nullptr));
    EXPECT_FALSE(rtsyn_spsc_command_try_pop(nullptr, &message));
    EXPECT_FALSE(rtsyn_spsc_command_try_pop(&queue, nullptr));
}

TEST_F(SPSCCommandTest, PreservesFifoOrderAndCapacity)
{
    rtsyn_spsc_command_queue_t queue;
    rtsyn_spsc_command_init(&queue);

    rtsyn_spsc_command_message_t message = {};
    EXPECT_FALSE(rtsyn_spsc_command_try_pop(&queue, &message));
    EXPECT_EQ(rtsyn_spsc_command_capacity(), RTSYN_SPSC_COMMAND_CAPACITY);
    EXPECT_EQ(rtsyn_spsc_command_size(&queue), 0U);

    for (size_t i = 0; i < RTSYN_SPSC_COMMAND_CAPACITY; ++i)
    {
        message.seq = i;
        message.timestamp_ns = i * 10;
        message.type = RTSYN_SPSC_COMMAND_MESSAGE_TYPE_GLOBAL_COMMAND;
        message.data.global_command.command = static_cast<uint8_t>(i % 255);
        ASSERT_TRUE(rtsyn_spsc_command_try_push(&queue, &message));
    }

    EXPECT_EQ(rtsyn_spsc_command_size(&queue), RTSYN_SPSC_COMMAND_CAPACITY);
    EXPECT_FALSE(rtsyn_spsc_command_try_push(&queue, &message));

    for (size_t i = 0; i < RTSYN_SPSC_COMMAND_CAPACITY; ++i)
    {
        ASSERT_TRUE(rtsyn_spsc_command_try_pop(&queue, &message));
        EXPECT_EQ(message.seq, i);
        EXPECT_EQ(message.timestamp_ns, i * 10);
        EXPECT_EQ(message.data.global_command.command, static_cast<uint8_t>(i % 255));
    }

    EXPECT_EQ(rtsyn_spsc_command_size(&queue), 0U);
    EXPECT_FALSE(rtsyn_spsc_command_try_pop(&queue, &message));
}

TEST_F(SPSCCommandTest, TransfersBetweenProcesses)
{
    const std::string name = make_shm_name();
    rtsyn_spsc_command_shared_unlink(name.c_str());

    rtsyn_spsc_command_shared_t consumer = {};
    ASSERT_EQ(rtsyn_spsc_command_shared_create(&consumer, name.c_str()), 0) << std::strerror(errno);

    pid_t pid = fork();
    ASSERT_NE(pid, -1);

    if (pid == 0)
    {
        rtsyn_spsc_command_shared_t producer = {};
        bool ok = rtsyn_spsc_command_shared_open(&producer, name.c_str()) == 0;

        for (uint64_t i = 0; ok && i < 256; ++i)
        {
            rtsyn_spsc_command_message_t message = {};
            message.seq = i;
            message.timestamp_ns = i * 100;
            message.type = RTSYN_SPSC_COMMAND_MESSAGE_TYPE_REQUEST_PORT_VALUES;
            message.data.plugin_request_ports.plugin_id = 42;
            message.data.plugin_request_ports.send = true;
            message.data.plugin_request_ports.portsyn_mask = 1ULL << (i % 32);

            while (!rtsyn_spsc_command_try_push(producer.queue, &message))
            {
                sleep_briefly();
            }
        }

        rtsyn_spsc_command_shared_close(&producer);
        child_exit(ok);
    }

    for (uint64_t i = 0; i < 256; ++i)
    {
        rtsyn_spsc_command_message_t message = {};
        while (!rtsyn_spsc_command_try_pop(consumer.queue, &message))
        {
            sleep_briefly();
        }

        EXPECT_EQ(message.seq, i);
        EXPECT_EQ(message.timestamp_ns, i * 100);
        EXPECT_EQ(message.type, RTSYN_SPSC_COMMAND_MESSAGE_TYPE_REQUEST_PORT_VALUES);
        EXPECT_EQ(message.data.plugin_request_ports.plugin_id, 42U);
        EXPECT_TRUE(message.data.plugin_request_ports.send);
        EXPECT_EQ(message.data.plugin_request_ports.portsyn_mask, 1ULL << (i % 32));
    }

    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), EXIT_SUCCESS);

    rtsyn_spsc_command_shared_close(&consumer);
    EXPECT_EQ(rtsyn_spsc_command_shared_unlink(name.c_str()), 0);
}

TEST_F(SPSCCommandTest, PushesSetParamCommand)
{
    rtsyn_spsc_command_queue_t queue = {};
    rtsyn_spsc_command_init(&queue);

    rtsyn_spsc_command_message_t message = {};
    message.seq = 7;
    message.type = RTSYN_SPSC_COMMAND_MESSAGE_TYPE_SET_PARAM;
    message.data.set_param.node_id = 3;
    message.data.set_param.param_id = 2;
    message.data.set_param.value_type = RTSYN_ABI_VALUE_F64;
    message.data.set_param.value.f64 = 12.5;

    ASSERT_TRUE(rtsyn_spsc_command_try_push(&queue, &message));

    rtsyn_spsc_command_message_t popped = {};
    ASSERT_TRUE(rtsyn_spsc_command_try_pop(&queue, &popped));
    EXPECT_EQ(popped.seq, 7U);
    EXPECT_EQ(popped.type, RTSYN_SPSC_COMMAND_MESSAGE_TYPE_SET_PARAM);
    EXPECT_EQ(popped.data.set_param.node_id, 3U);
    EXPECT_EQ(popped.data.set_param.param_id, 2U);
    EXPECT_EQ(popped.data.set_param.value_type, RTSYN_ABI_VALUE_F64);
    EXPECT_DOUBLE_EQ(popped.data.set_param.value.f64, 12.5);
}

TEST_F(SPSCCommandTest, PushesSetRuntimePeriodCommand)
{
    rtsyn_spsc_command_queue_t queue = {};
    rtsyn_spsc_command_init(&queue);

    rtsyn_spsc_command_message_t message = {};
    message.seq = 8;
    message.type = RTSYN_SPSC_COMMAND_MESSAGE_TYPE_SET_RUNTIME_PERIOD;
    message.data.set_runtime_period.period_ns = 500000;

    ASSERT_TRUE(rtsyn_spsc_command_try_push(&queue, &message));

    rtsyn_spsc_command_message_t popped = {};
    ASSERT_TRUE(rtsyn_spsc_command_try_pop(&queue, &popped));
    EXPECT_EQ(popped.seq, 8U);
    EXPECT_EQ(popped.type, RTSYN_SPSC_COMMAND_MESSAGE_TYPE_SET_RUNTIME_PERIOD);
    EXPECT_EQ(popped.data.set_runtime_period.period_ns, 500000U);
}

TEST_F(SPSCCommandTest, PushesLoadNodeCommand)
{
    rtsyn_spsc_command_queue_t queue = {};
    rtsyn_spsc_command_init(&queue);

    rtsyn_spsc_command_message_t message = {};
    message.seq = 8;
    message.type = RTSYN_SPSC_COMMAND_MESSAGE_TYPE_LOAD_NODE;
    message.data.load_node.node_type = RTSYN_ABI_NODE_PLUGIN;
    snprintf(message.data.load_node.module_path, sizeof(message.data.load_node.module_path),
             "/tmp/plugin.so");

    ASSERT_TRUE(rtsyn_spsc_command_try_push(&queue, &message));

    rtsyn_spsc_command_message_t popped = {};
    ASSERT_TRUE(rtsyn_spsc_command_try_pop(&queue, &popped));
    EXPECT_EQ(popped.seq, 8U);
    EXPECT_EQ(popped.type, RTSYN_SPSC_COMMAND_MESSAGE_TYPE_LOAD_NODE);
    EXPECT_EQ(popped.data.load_node.node_type, RTSYN_ABI_NODE_PLUGIN);
    EXPECT_STREQ(popped.data.load_node.module_path, "/tmp/plugin.so");
}

TEST_F(SPSCCommandTest, PushesAddNodeCommand)
{
    rtsyn_spsc_command_queue_t queue = {};
    rtsyn_spsc_command_init(&queue);

    rtsyn_spsc_command_message_t message = {};
    message.seq = 9;
    message.type = RTSYN_SPSC_COMMAND_MESSAGE_TYPE_ADD_NODE;
    message.data.add_node.node_type = RTSYN_ABI_NODE_DEVICE;
    snprintf(message.data.add_node.node_name, sizeof(message.data.add_node.node_name),
             "device-module");

    ASSERT_TRUE(rtsyn_spsc_command_try_push(&queue, &message));

    rtsyn_spsc_command_message_t popped = {};
    ASSERT_TRUE(rtsyn_spsc_command_try_pop(&queue, &popped));
    EXPECT_EQ(popped.seq, 9U);
    EXPECT_EQ(popped.type, RTSYN_SPSC_COMMAND_MESSAGE_TYPE_ADD_NODE);
    EXPECT_EQ(popped.data.add_node.node_type, RTSYN_ABI_NODE_DEVICE);
    EXPECT_STREQ(popped.data.add_node.node_name, "device-module");
}

TEST_F(SPSCCommandTest, PushesAddAndRemoveConnectionCommands)
{
    rtsyn_spsc_command_queue_t queue = {};
    rtsyn_spsc_command_init(&queue);

    rtsyn_spsc_command_message_t message = {};
    message.seq = 10;
    message.type = RTSYN_SPSC_COMMAND_MESSAGE_TYPE_ADD_CONNECTION;
    message.data.add_connection.connection_id = 13;
    message.data.add_connection.source_node_id = 1;
    message.data.add_connection.source_port_id = 2;
    message.data.add_connection.destination_node_id = 3;
    message.data.add_connection.destination_port_id = 4;
    ASSERT_TRUE(rtsyn_spsc_command_try_push(&queue, &message));

    message = {};
    message.seq = 11;
    message.type = RTSYN_SPSC_COMMAND_MESSAGE_TYPE_REMOVE_CONNECTION;
    message.data.remove_connection.connection_id = 13;
    ASSERT_TRUE(rtsyn_spsc_command_try_push(&queue, &message));

    rtsyn_spsc_command_message_t popped = {};
    ASSERT_TRUE(rtsyn_spsc_command_try_pop(&queue, &popped));
    EXPECT_EQ(popped.seq, 10U);
    EXPECT_EQ(popped.type, RTSYN_SPSC_COMMAND_MESSAGE_TYPE_ADD_CONNECTION);
    EXPECT_EQ(popped.data.add_connection.connection_id, 13U);
    EXPECT_EQ(popped.data.add_connection.source_node_id, 1U);
    EXPECT_EQ(popped.data.add_connection.source_port_id, 2U);
    EXPECT_EQ(popped.data.add_connection.destination_node_id, 3U);
    EXPECT_EQ(popped.data.add_connection.destination_port_id, 4U);

    ASSERT_TRUE(rtsyn_spsc_command_try_pop(&queue, &popped));
    EXPECT_EQ(popped.seq, 11U);
    EXPECT_EQ(popped.type, RTSYN_SPSC_COMMAND_MESSAGE_TYPE_REMOVE_CONNECTION);
    EXPECT_EQ(popped.data.remove_connection.connection_id, 13U);
}

TEST_F(SPSCCommandTest, LockedSharedMappingCanTransferWhenPermitted)
{
    const std::string name = make_shm_name() + "_locked";
    rtsyn_spsc_command_shared_unlink(name.c_str());

    rtsyn_spsc_command_shared_t producer = {};
    if (rtsyn_spsc_command_shared_create_locked(&producer, name.c_str()) != 0)
    {
        int saved_errno = errno;
        rtsyn_spsc_command_shared_unlink(name.c_str());
        if (saved_errno == ENOMEM || saved_errno == EPERM)
        {
            GTEST_SKIP() << "mlock is not permitted by this environment: "
                         << std::strerror(saved_errno);
        }
        FAIL() << std::strerror(saved_errno);
    }

    rtsyn_spsc_command_shared_t consumer = {};
    if (rtsyn_spsc_command_shared_open_locked(&consumer, name.c_str()) != 0)
    {
        int saved_errno = errno;
        rtsyn_spsc_command_shared_close(&producer);
        rtsyn_spsc_command_shared_unlink(name.c_str());
        if (saved_errno == ENOMEM || saved_errno == EPERM)
        {
            GTEST_SKIP() << "mlock is not permitted by this environment: "
                         << std::strerror(saved_errno);
        }
        FAIL() << std::strerror(saved_errno);
    }

    rtsyn_spsc_command_message_t message = {};
    message.seq = 1;
    message.type = RTSYN_SPSC_COMMAND_MESSAGE_TYPE_GLOBAL_COMMAND;
    message.data.global_command.command = 7;

    ASSERT_TRUE(rtsyn_spsc_command_try_push(producer.queue, &message));
    message = {};
    ASSERT_TRUE(rtsyn_spsc_command_try_pop(consumer.queue, &message));
    EXPECT_EQ(message.seq, 1U);
    EXPECT_EQ(message.data.global_command.command, 7U);

    rtsyn_spsc_command_shared_close(&consumer);
    rtsyn_spsc_command_shared_close(&producer);
    EXPECT_EQ(rtsyn_spsc_command_shared_unlink(name.c_str()), 0);
}
