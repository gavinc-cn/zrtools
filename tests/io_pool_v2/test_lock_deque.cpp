#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "zrtools/io_pool_v2/lock_deque.h"

namespace {

TEST(LockDequeTest, PopFromEmptyReturnsFalse) {
    LockDeque<int> q;
    int out = 42;
    EXPECT_FALSE(q.pop_front(out));
    EXPECT_EQ(out, 42);  // out 未被修改
}

TEST(LockDequeTest, PushPopPreservesFIFOOrder) {
    LockDeque<int> q;
    EXPECT_TRUE(q.push_back(1));
    EXPECT_TRUE(q.push_back(2));
    EXPECT_TRUE(q.push_back(3));

    int v = 0;
    EXPECT_TRUE(q.pop_front(v)); EXPECT_EQ(v, 1);
    EXPECT_TRUE(q.pop_front(v)); EXPECT_EQ(v, 2);
    EXPECT_TRUE(q.pop_front(v)); EXPECT_EQ(v, 3);
    EXPECT_FALSE(q.pop_front(v));
}

TEST(LockDequeTest, PushBackAlwaysReturnsTrue) {
    LockDeque<int> q;
    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(q.push_back(i));
    }
}

TEST(LockDequeTest, ConcurrentProducersSingleConsumer) {
    LockDeque<int> q;
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 10'000;

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&q, p]{
            for (int i = 0; i < kPerProducer; ++i) {
                q.push_back(p * kPerProducer + i);
            }
        });
    }
    for (auto& t : producers) t.join();

    std::vector<bool> seen(kProducers * kPerProducer, false);
    int popped = 0;
    int v = 0;
    while (q.pop_front(v)) {
        ASSERT_GE(v, 0);
        ASSERT_LT(v, static_cast<int>(seen.size()));
        ASSERT_FALSE(seen[v]) << "duplicate value " << v;
        seen[v] = true;
        ++popped;
    }
    EXPECT_EQ(popped, kProducers * kPerProducer);
    for (bool b : seen) EXPECT_TRUE(b);
}

TEST(LockDequeTest, ConcurrentProducerAndConsumer) {
    LockDeque<int> q;
    constexpr int kCount = 50'000;

    std::atomic<int> consumed_sum{0};
    std::atomic<bool> producer_done{false};

    std::thread consumer([&]{
        int v = 0;
        int got = 0;
        while (got < kCount) {
            if (q.pop_front(v)) {
                consumed_sum += v;
                ++got;
            } else if (producer_done.load()) {
                // 最后一轮清空
                while (q.pop_front(v)) {
                    consumed_sum += v;
                    ++got;
                }
                break;
            } else {
                std::this_thread::yield();
            }
        }
    });

    std::thread producer([&]{
        for (int i = 1; i <= kCount; ++i) q.push_back(i);
        producer_done = true;
    });

    producer.join();
    consumer.join();

    // 1..kCount 的和
    const long long expected = static_cast<long long>(kCount) * (kCount + 1) / 2;
    EXPECT_EQ(consumed_sum.load(), expected);
}

}  // namespace
