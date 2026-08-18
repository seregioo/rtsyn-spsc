#include <gtest/gtest.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <unistd.h>

#include "rtsyn/spsc/result/spsc.h"

namespace {

std::string
result_queue_name()
{
    return std::string("/rtsyn_spsc_result_") + std::to_string(getpid());
}

} // namespace

static_assert(sizeof(decltype(rtsyn_spsc_result_queue_t::head)) == sizeof(uint64_t));
static_assert(sizeof(decltype(rtsyn_spsc_result_queue_t::tail)) == sizeof(uint64_t));

TEST(ResultSpscTest, RejectsInvalidArguments)
{
    rtsyn_spsc_result_queue_t queue;
    rtsyn_spsc_result_message_t message = {};
    rtsyn_spsc_result_init(&queue);

    EXPECT_FALSE(rtsyn_spsc_result_try_push(nullptr, &message));
    EXPECT_FALSE(rtsyn_spsc_result_try_push(&queue, nullptr));
    EXPECT_FALSE(rtsyn_spsc_result_try_pop(nullptr, &message));
    EXPECT_FALSE(rtsyn_spsc_result_try_pop(&queue, nullptr));
}

TEST(ResultSpscTest, PreservesFifoOrderAndCapacity)
{
    rtsyn_spsc_result_queue_t queue;
    rtsyn_spsc_result_init(&queue);

    rtsyn_spsc_result_message_t message = {};
    EXPECT_FALSE(rtsyn_spsc_result_try_pop(&queue, &message));
    EXPECT_EQ(rtsyn_spsc_result_capacity(), RTSYN_SPSC_RESULT_CAPACITY);
    EXPECT_EQ(rtsyn_spsc_result_size(&queue), 0U);

    for (uint64_t i = 0; i < RTSYN_SPSC_RESULT_CAPACITY; i++)
    {
        message.seq = i + 1;
        message.status = RTSYN_SPSC_RESULT_STATUS_OK;
        ASSERT_TRUE(rtsyn_spsc_result_try_push(&queue, &message));
    }

    EXPECT_EQ(rtsyn_spsc_result_size(&queue), RTSYN_SPSC_RESULT_CAPACITY);
    EXPECT_FALSE(rtsyn_spsc_result_try_push(&queue, &message));

    for (uint64_t i = 0; i < RTSYN_SPSC_RESULT_CAPACITY; i++)
    {
        ASSERT_TRUE(rtsyn_spsc_result_try_pop(&queue, &message));
        EXPECT_EQ(message.seq, i + 1);
        EXPECT_EQ(message.status, RTSYN_SPSC_RESULT_STATUS_OK);
    }

    EXPECT_EQ(rtsyn_spsc_result_size(&queue), 0U);
    EXPECT_FALSE(rtsyn_spsc_result_try_pop(&queue, &message));
}

TEST(ResultSpscTest, SharedMemoryCreateOpenAndUnlink)
{
    const std::string name = result_queue_name();
    rtsyn_spsc_result_shared_unlink(name.c_str());

    rtsyn_spsc_result_shared_t producer = {};
    ASSERT_EQ(rtsyn_spsc_result_shared_create(&producer, name.c_str()), 0)
        << std::strerror(errno);

    rtsyn_spsc_result_shared_t consumer = {};
    ASSERT_EQ(rtsyn_spsc_result_shared_open(&consumer, name.c_str()), 0)
        << std::strerror(errno);

    rtsyn_spsc_result_message_t message = {};
    message.seq = 7;
    message.command_type = RTSYN_SPSC_COMMAND_MESSAGE_TYPE_LOAD_NODE;
    message.status = RTSYN_SPSC_RESULT_STATUS_OK;
    snprintf(message.node.name, sizeof(message.node.name), "%s", "adder");

    ASSERT_TRUE(rtsyn_spsc_result_try_push(producer.queue, &message));

    rtsyn_spsc_result_message_t received = {};
    ASSERT_TRUE(rtsyn_spsc_result_try_pop(consumer.queue, &received));
    EXPECT_EQ(received.seq, 7U);
    EXPECT_EQ(received.command_type, RTSYN_SPSC_COMMAND_MESSAGE_TYPE_LOAD_NODE);
    EXPECT_EQ(received.status, RTSYN_SPSC_RESULT_STATUS_OK);
    EXPECT_STREQ(received.node.name, "adder");

    rtsyn_spsc_result_shared_close(&consumer);
    rtsyn_spsc_result_shared_close(&producer);
    EXPECT_EQ(rtsyn_spsc_result_shared_unlink(name.c_str()), 0);
}
