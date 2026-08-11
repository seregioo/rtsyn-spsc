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
#include "rtsyn/spsc/telemetry/spsc.h"
#include "rtsyn/spsc/telemetry/values.h"
}

namespace {

std::string
make_shm_name()
{
    return std::string("/rtsyn_spsc_telemetry_") + std::to_string(getpid());
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

class SPSCTelemetryValuesTest : public ::testing::Test {};

static_assert(sizeof(decltype(rtsyn_spsc_telemetry_values_t::head)) == sizeof(uint64_t));
static_assert(sizeof(decltype(rtsyn_spsc_telemetry_values_t::tail)) == sizeof(uint64_t));

TEST_F(SPSCTelemetryValuesTest, ValuesPreserveRanges)
{
    static rtsyn_spsc_telemetry_values_t values;
    rtsyn_spsc_telemetry_values_init(&values);

    rtsyn_spsc_telemetry_value_t samples[3] = {};
    for (uint32_t i = 0; i < 3; ++i)
    {
        samples[i].cycle_id = 4;
        samples[i].timestamp_ns = 100 + i;
        samples[i].node_id = 99;
        samples[i].value_id = i;
        samples[i].sample_offset = static_cast<uint16_t>(i);
        samples[i].value_kind = RTSYN_SPSC_TELEMETRY_VALUE_KIND_PORT;
        samples[i].source = RTSYN_SPSC_TELEMETRY_SOURCE_DEVICE;
        samples[i].value_type = RTSYN_ABI_VALUE_F64;
        samples[i].data.f64 = 10.0 + i;
    }

    uint64_t start_index = 0;
    ASSERT_TRUE(rtsyn_spsc_telemetry_values_try_push(&values, samples, 3, &start_index));
    EXPECT_EQ(start_index, 0U);
    EXPECT_EQ(rtsyn_spsc_telemetry_values_size(&values), 3U);

    for (uint64_t i = 0; i < 3; ++i)
    {
        rtsyn_spsc_telemetry_value_t sample = {};
        ASSERT_TRUE(rtsyn_spsc_telemetry_values_try_get(&values, start_index + i, &sample));
        EXPECT_EQ(sample.cycle_id, 4U);
        EXPECT_EQ(sample.node_id, 99U);
        EXPECT_EQ(sample.value_id, i);
        EXPECT_EQ(sample.value_kind, RTSYN_SPSC_TELEMETRY_VALUE_KIND_PORT);
        EXPECT_EQ(sample.source, RTSYN_SPSC_TELEMETRY_SOURCE_DEVICE);
        EXPECT_EQ(sample.value_type, RTSYN_ABI_VALUE_F64);
        EXPECT_DOUBLE_EQ(sample.data.f64, 10.0 + static_cast<double>(i));
    }

    EXPECT_TRUE(rtsyn_spsc_telemetry_values_release(&values, 3));
    EXPECT_EQ(rtsyn_spsc_telemetry_values_size(&values), 0U);
}

TEST_F(SPSCTelemetryValuesTest, ValuesRejectInvalidOperations)
{
    static rtsyn_spsc_telemetry_values_t values;
    rtsyn_spsc_telemetry_values_init(&values);

    rtsyn_spsc_telemetry_value_t sample = {};
    uint64_t start_index = 0;

    EXPECT_EQ(rtsyn_spsc_telemetry_values_capacity(), RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY);
    EXPECT_EQ(rtsyn_spsc_telemetry_values_free_space(&values),
              RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY);
    EXPECT_FALSE(rtsyn_spsc_telemetry_values_try_push(nullptr, &sample, 1, &start_index));
    EXPECT_FALSE(rtsyn_spsc_telemetry_values_try_push(&values, nullptr, 1, &start_index));
    EXPECT_FALSE(rtsyn_spsc_telemetry_values_try_get(nullptr, 0, &sample));
    EXPECT_FALSE(rtsyn_spsc_telemetry_values_try_get(&values, 0, nullptr));
    EXPECT_FALSE(rtsyn_spsc_telemetry_values_try_get(&values, 0, &sample));
    EXPECT_FALSE(rtsyn_spsc_telemetry_values_release(nullptr, 1));
    EXPECT_FALSE(rtsyn_spsc_telemetry_values_release(&values, 1));
}

TEST_F(SPSCTelemetryValuesTest, ValuesDetectFullAndRecoverAfterRelease)
{
    static rtsyn_spsc_telemetry_values_t values;
    rtsyn_spsc_telemetry_values_init(&values);

    rtsyn_spsc_telemetry_value_t sample = {};
    sample.cycle_id = 1;
    sample.node_id = 2;
    sample.source = RTSYN_SPSC_TELEMETRY_SOURCE_PLUGIN;
    sample.value_type = RTSYN_ABI_VALUE_U64;

    for (size_t i = 0; i < RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY; ++i)
    {
        sample.value_id = static_cast<uint32_t>(i);
        sample.data.u64 = i;
        ASSERT_TRUE(rtsyn_spsc_telemetry_values_try_push(&values, &sample, 1, nullptr));
    }

    EXPECT_EQ(rtsyn_spsc_telemetry_values_free_space(&values), 0U);
    EXPECT_FALSE(rtsyn_spsc_telemetry_values_try_push(&values, &sample, 1, nullptr));

    EXPECT_TRUE(rtsyn_spsc_telemetry_values_release(&values, 1));
    EXPECT_EQ(rtsyn_spsc_telemetry_values_free_space(&values), 1U);

    sample.value_id = 1234;
    sample.data.u64 = 1234;
    uint64_t start_index = 0;
    EXPECT_TRUE(rtsyn_spsc_telemetry_values_try_push(&values, &sample, 1, &start_index));
    EXPECT_EQ(start_index, RTSYN_SPSC_TELEMETRY_VALUES_CAPACITY);
}

TEST_F(SPSCTelemetryValuesTest, PublishesValueRangeWithEvent)
{
    rtsyn_spsc_telemetry_queue_t events;
    static rtsyn_spsc_telemetry_values_t values;
    rtsyn_spsc_telemetry_init(&events);
    rtsyn_spsc_telemetry_values_init(&values);

    rtsyn_spsc_telemetry_value_t samples[2] = {};
    samples[0].cycle_id = 8;
    samples[0].node_id = 3;
    samples[0].value_id = 1;
    samples[0].source = RTSYN_SPSC_TELEMETRY_SOURCE_PLUGIN;
    samples[0].value_type = RTSYN_ABI_VALUE_F32;
    samples[0].data.f32 = 1.5F;
    samples[1] = samples[0];
    samples[1].value_id = 2;
    samples[1].data.f32 = 2.5F;

    rtsyn_spsc_telemetry_message_t event = {};
    event.seq = 11;
    event.timestamp_ns = 1000;
    event.data.values_written.cycle_id = 8;
    event.data.values_written.node_id = 3;
    event.data.values_written.source = RTSYN_SPSC_TELEMETRY_SOURCE_PLUGIN;

    ASSERT_TRUE(rtsyn_spsc_telemetry_try_publish_values(&events, &values, &event, samples, 2));

    rtsyn_spsc_telemetry_message_t received_event = {};
    ASSERT_TRUE(rtsyn_spsc_telemetry_try_pop(&events, &received_event));
    EXPECT_EQ(received_event.type, RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_VALUES_WRITTEN);
    EXPECT_EQ(received_event.data.values_written.cycle_id, 8U);
    EXPECT_EQ(received_event.data.values_written.node_id, 3U);
    EXPECT_EQ(received_event.data.values_written.value_count, 2U);

    rtsyn_spsc_telemetry_value_t received_value = {};
    ASSERT_TRUE(rtsyn_spsc_telemetry_values_try_get(
        &values, received_event.data.values_written.values_start_index + 1, &received_value));
    EXPECT_EQ(received_value.value_id, 2U);
    EXPECT_FLOAT_EQ(received_value.data.f32, 2.5F);

    EXPECT_TRUE(rtsyn_spsc_telemetry_values_release(
        &values, received_event.data.values_written.value_count));
}

TEST_F(SPSCTelemetryValuesTest, PublishRejectsInvalidArguments)
{
    rtsyn_spsc_telemetry_queue_t events;
    static rtsyn_spsc_telemetry_values_t values;
    rtsyn_spsc_telemetry_message_t event = {};
    rtsyn_spsc_telemetry_value_t sample = {};

    rtsyn_spsc_telemetry_init(&events);
    rtsyn_spsc_telemetry_values_init(&values);

    EXPECT_FALSE(rtsyn_spsc_telemetry_try_publish_values(nullptr, &values, &event, &sample, 1));
    EXPECT_FALSE(rtsyn_spsc_telemetry_try_publish_values(&events, nullptr, &event, &sample, 1));
    EXPECT_FALSE(rtsyn_spsc_telemetry_try_publish_values(&events, &values, nullptr, &sample, 1));
    EXPECT_FALSE(rtsyn_spsc_telemetry_try_publish_values(&events, &values, &event, nullptr, 1));
    EXPECT_FALSE(rtsyn_spsc_telemetry_try_publish_values(&events, &values, &event, &sample, 0));
}

TEST_F(SPSCTelemetryValuesTest, PublishDoesNotWriteValuesWhenEventQueueIsFull)
{
    rtsyn_spsc_telemetry_queue_t events;
    static rtsyn_spsc_telemetry_values_t values;
    rtsyn_spsc_telemetry_init(&events);
    rtsyn_spsc_telemetry_values_init(&values);

    rtsyn_spsc_telemetry_message_t filler = {};
    filler.type = RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_NODE_STATUS;
    for (size_t i = 0; i < RTSYN_SPSC_TELEMETRY_CAPACITY; ++i)
    {
        filler.seq = i;
        ASSERT_TRUE(rtsyn_spsc_telemetry_try_push(&events, &filler));
    }

    rtsyn_spsc_telemetry_value_t sample = {};
    sample.cycle_id = 1;
    sample.node_id = 2;
    sample.value_id = 3;
    sample.source = RTSYN_SPSC_TELEMETRY_SOURCE_DEVICE;
    sample.value_type = RTSYN_ABI_VALUE_F64;
    sample.data.f64 = 4.0;

    rtsyn_spsc_telemetry_message_t event = {};
    event.data.values_written.cycle_id = 1;
    event.data.values_written.node_id = 2;
    event.data.values_written.source = RTSYN_SPSC_TELEMETRY_SOURCE_DEVICE;

    EXPECT_FALSE(rtsyn_spsc_telemetry_try_publish_values(&events, &values, &event, &sample, 1));
    EXPECT_EQ(rtsyn_spsc_telemetry_values_size(&values), 0U);
}

TEST_F(SPSCTelemetryValuesTest, TransfersBetweenProcesses)
{
    const std::string events_name = make_shm_name() + "_events";
    const std::string values_name = make_shm_name() + "_values";
    rtsyn_spsc_telemetry_shared_unlink(events_name.c_str());
    rtsyn_spsc_telemetry_values_shared_unlink(values_name.c_str());

    rtsyn_spsc_telemetry_shared_t producer_events = {};
    ASSERT_EQ(rtsyn_spsc_telemetry_shared_create(&producer_events, events_name.c_str()), 0)
        << std::strerror(errno);

    rtsyn_spsc_telemetry_values_shared_t producer_values = {};
    ASSERT_EQ(rtsyn_spsc_telemetry_values_shared_create(&producer_values, values_name.c_str()), 0)
        << std::strerror(errno);

    pid_t pid = fork();
    ASSERT_NE(pid, -1);

    if (pid == 0)
    {
        rtsyn_spsc_telemetry_shared_t consumer_events = {};
        rtsyn_spsc_telemetry_values_shared_t consumer_values = {};
        bool ok =
            rtsyn_spsc_telemetry_shared_open(&consumer_events, events_name.c_str()) == 0
            && rtsyn_spsc_telemetry_values_shared_open(&consumer_values, values_name.c_str()) == 0;

        for (uint64_t i = 0; ok && i < 128; ++i)
        {
            rtsyn_spsc_telemetry_message_t message = {};
            while (!rtsyn_spsc_telemetry_try_pop(consumer_events.queue, &message))
            {
                sleep_briefly();
            }

            ok = message.seq == i
                 && message.type == RTSYN_SPSC_TELEMETRY_MESSAGE_TYPE_VALUES_WRITTEN
                 && message.data.values_written.value_count == 2U;

            for (uint32_t j = 0; ok && j < message.data.values_written.value_count; ++j)
            {
                rtsyn_spsc_telemetry_value_t value = {};
                ok = rtsyn_spsc_telemetry_values_try_get(
                    consumer_values.values, message.data.values_written.values_start_index + j,
                    &value);
                ok = ok && value.cycle_id == i && value.node_id == 7U && value.value_id == j
                     && value.value_type == RTSYN_ABI_VALUE_F64;
            }

            ok = ok
                 && rtsyn_spsc_telemetry_values_release(consumer_values.values,
                                                        message.data.values_written.value_count);
        }

        rtsyn_spsc_telemetry_values_shared_close(&consumer_values);
        rtsyn_spsc_telemetry_shared_close(&consumer_events);
        child_exit(ok);
    }

    for (uint64_t i = 0; i < 128; ++i)
    {
        rtsyn_spsc_telemetry_value_t values[2] = {};
        for (uint32_t j = 0; j < 2; ++j)
        {
            values[j].cycle_id = i;
            values[j].timestamp_ns = i * 1000 + j;
            values[j].node_id = 7;
            values[j].value_id = j;
            values[j].sample_offset = static_cast<uint16_t>(j);
            values[j].source = RTSYN_SPSC_TELEMETRY_SOURCE_PLUGIN;
            values[j].value_type = RTSYN_ABI_VALUE_F64;
            values[j].data.f64 = static_cast<double>(i + j);
        }

        rtsyn_spsc_telemetry_message_t message = {};
        message.seq = i;
        message.timestamp_ns = i * 1000;
        message.data.values_written.cycle_id = i;
        message.data.values_written.node_id = 7;
        message.data.values_written.source = RTSYN_SPSC_TELEMETRY_SOURCE_PLUGIN;

        while (!rtsyn_spsc_telemetry_try_publish_values(
            producer_events.queue, producer_values.values, &message, values, 2))
        {
            sleep_briefly();
        }
    }

    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), EXIT_SUCCESS);

    rtsyn_spsc_telemetry_values_shared_close(&producer_values);
    rtsyn_spsc_telemetry_shared_close(&producer_events);
    EXPECT_EQ(rtsyn_spsc_telemetry_values_shared_unlink(values_name.c_str()), 0);
    EXPECT_EQ(rtsyn_spsc_telemetry_shared_unlink(events_name.c_str()), 0);
}
