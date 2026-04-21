#include <gtest/gtest.h>

#include <atomic>
#include <thread>

#include "zrtools/io_pool_v2/sync_thread.h"

namespace {

TEST(SyncThreadTest, PostExecutesImmediatelyInCallerThread) {
    SyncThread t;
    ASSERT_TRUE(t.ThreadStart());

    const std::thread::id caller_id = std::this_thread::get_id();
    std::thread::id observed_id{};
    bool ran = false;

    t.Post([&]{
        ran = true;
        observed_id = std::this_thread::get_id();
    });

    EXPECT_TRUE(ran);
    EXPECT_EQ(observed_id, caller_id);

    EXPECT_TRUE(t.ThreadStop());
}

TEST(SyncThreadTest, IsInThreadAlwaysTrue) {
    SyncThread t;
    EXPECT_TRUE(t.IsInThread());  // 启动前

    t.ThreadStart();
    EXPECT_TRUE(t.IsInThread());

    bool in_from_worker = false;
    t.Post([&]{ in_from_worker = t.IsInThread(); });
    EXPECT_TRUE(in_from_worker);

    // 从其他线程观察也应为 true（SyncThread 约定）
    std::atomic<bool> in_from_other{false};
    std::thread other([&]{ in_from_other = t.IsInThread(); });
    other.join();
    EXPECT_TRUE(in_from_other.load());

    t.ThreadStop();
}

TEST(SyncThreadTest, StartStopIsIdempotent) {
    SyncThread t;
    EXPECT_TRUE(t.ThreadStart());
    EXPECT_TRUE(t.ThreadStart());  // 重入应安全
    EXPECT_TRUE(t.ThreadStop());
    EXPECT_TRUE(t.ThreadStop());   // 停后再停
    EXPECT_TRUE(t.ThreadStart());  // 可重新启动
    EXPECT_TRUE(t.ThreadStop());
}

TEST(SyncThreadTest, RefIoServiceReturnsNull) {
    SyncThread t;
    EXPECT_EQ(t.RefIoService(), nullptr);
}

}  // namespace
