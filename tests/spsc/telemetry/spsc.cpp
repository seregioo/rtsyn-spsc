#include <gtest/gtest.h>

extern "C" {
#include "rtsyn/spsc/telemetry/spsc.h"
}

class SPSCTelemetryTest : public ::testing::Test {};

static_assert(sizeof(decltype(rtsyn_spsc_telemetry_queue_t::head)) == sizeof(uint64_t));
static_assert(sizeof(decltype(rtsyn_spsc_telemetry_queue_t::tail)) == sizeof(uint64_t));

TEST_F(SPSCTelemetryTest, RejectsNullArguments)
{
    rtsyn_spsc_telemetry_queue_t queue;
    rtsyn_spsc_telemetry_message_t message = {};

    rtsyn_spsc_telemetry_init(&queue);

    EXPECT_FALSE(rtsyn_spsc_telemetry_try_push(nullptr, &message));
    EXPECT_FALSE(rtsyn_spsc_telemetry_try_push(&queue, nullptr));
    EXPECT_FALSE(rtsyn_spsc_telemetry_try_pop(nullptr, &message));
    EXPECT_FALSE(rtsyn_spsc_telemetry_try_pop(&queue, nullptr));
}

TEST_F(SPSCTelemetryTest, PreservesFifoOrder)
{
    rtsyn_spsc_telemetry_queue_t queue;
    rtsyn_spsc_telemetry_init(&queue);

    rtsyn_spsc_telemetry_message_t message = {};

    for (size_t i = 0; i < 64; ++i)
    {
        message.seq = i;
        message.timestamp_ns = i + 1000;
        message.type = RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_NODE_STATUS;
        message.data.node_status.cycle_id = 10;
        message.data.node_status.node_id = static_cast<uint32_t>(i);
        message.data.node_status.source = RTSYN_SPSC_TELEMETRY_SOURCE_PLUGIN;
        message.data.node_status.status = RTSYN_SPSC_TELEMETRY_NODE_STATUS_OK;
        ASSERT_TRUE(rtsyn_spsc_telemetry_try_push(&queue, &message));
    }

    EXPECT_EQ(rtsyn_spsc_telemetry_size(&queue), 64U);

    for (size_t i = 0; i < 64; ++i)
    {
        ASSERT_TRUE(rtsyn_spsc_telemetry_try_pop(&queue, &message));
        EXPECT_EQ(message.seq, i);
        EXPECT_EQ(message.timestamp_ns, i + 1000);
        EXPECT_EQ(message.data.node_status.cycle_id, 10U);
        EXPECT_EQ(message.data.node_status.node_id, i);
        EXPECT_EQ(message.data.node_status.source, RTSYN_SPSC_TELEMETRY_SOURCE_PLUGIN);
        EXPECT_EQ(message.data.node_status.status, RTSYN_SPSC_TELEMETRY_NODE_STATUS_OK);
    }
}
