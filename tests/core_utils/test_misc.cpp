#include <gtest/gtest.h>
#include <cmath>
#include <tuple>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "zrtools/zrt_misc.h"

namespace {

TEST(MiscTest, SafeDivGuardsAgainstZero) {
    EXPECT_DOUBLE_EQ(zrt::safe_div(10.0, 2.0), 5.0);
    EXPECT_DOUBLE_EQ(zrt::safe_div(10.0, 0.0, 42.0), 42.0);
    EXPECT_TRUE(std::isnan(zrt::safe_div(1.0, 0.0)));  // 默认 NAN
}

TEST(MiscTest, GetRandomStaysWithinBounds) {
    for (int i = 0; i < 100; ++i) {
        const int v = zrt::get_random<int>(5, 10);
        EXPECT_GE(v, 5);
        EXPECT_LE(v, 10);
    }
}

TEST(MiscTest, GetUuidIsNotEmpty) {
    const auto u1 = zrt::get_uuid();
    const auto u2 = zrt::get_uuid();
    EXPECT_FALSE(u1.empty());
    EXPECT_NE(u1, u2);  // 随机生成极大概率不相等
}

TEST(MiscTest, GetMd5Deterministic) {
    const std::string input = "hello";
    const std::string h1 = zrt::get_md5(input.data(), input.size());
    const std::string h2 = zrt::get_md5(input.data(), input.size());
    EXPECT_EQ(h1, h2);
    EXPECT_EQ(h1.size(), 32U);  // 16 字节 → hex 32 字符
}

TEST(MiscTest, TupleHasherStableAcrossCalls) {
    zrt::TupleHasher hasher;
    const auto t1 = std::make_tuple(1, std::string("abc"), 3.14);
    const auto t2 = std::make_tuple(1, std::string("abc"), 3.14);
    EXPECT_EQ(hasher(t1), hasher(t2));

    std::unordered_set<size_t> seen;
    seen.insert(hasher(std::make_tuple(1, 2, 3)));
    seen.insert(hasher(std::make_tuple(1, 2, 4)));
    seen.insert(hasher(std::make_tuple(2, 1, 3)));
    EXPECT_EQ(seen.size(), 3U);  // 不同 tuple 撞哈希概率极低
}

TEST(MiscTest, PairHasherWorks) {
    zrt::PairHasher hasher;
    const std::pair<int, std::string> p1{1, "a"};
    const std::pair<int, std::string> p2{1, "a"};
    const std::pair<int, std::string> p3{2, "a"};
    EXPECT_EQ(hasher(p1), hasher(p2));
    EXPECT_NE(hasher(p1), hasher(p3));
}

TEST(MiscTest, GetMapVal) {
    const std::unordered_map<std::string, int> m{{"a", 1}, {"b", 2}};
    int out = 0;
    EXPECT_TRUE(zrt::get_map_val(m, std::string("a"), out));
    EXPECT_EQ(out, 1);
    out = 999;
    EXPECT_FALSE(zrt::get_map_val(m, std::string("missing"), out));
    EXPECT_EQ(out, 999);  // 未找到不会改 out
}

TEST(MiscTest, TryGetMapVal) {
    const std::unordered_map<int, std::string> m{{1, "x"}};
    std::string out;
    EXPECT_TRUE(zrt::TryGetMapVal(m, 1, out));
    EXPECT_EQ(out, "x");
    out = "unchanged";
    EXPECT_FALSE(zrt::TryGetMapVal(m, 99, out));
    EXPECT_EQ(out, "unchanged");
}

TEST(MiscTest, GetMapValReturnsDefault) {
    const std::unordered_map<std::string, int> m{{"a", 1}};
    EXPECT_EQ(zrt::GetUmapVal(m, std::string("a"), -1), 1);
    EXPECT_EQ(zrt::GetUmapVal(m, std::string("missing"), -1), -1);
}

TEST(MiscTest, EqualAnyOfAndAllEqual) {
    EXPECT_TRUE(zrt::equal_any_of(3, 1, 2, 3, 4));
    EXPECT_FALSE(zrt::equal_any_of(5, 1, 2, 3, 4));

    EXPECT_TRUE(zrt::all_equal(7, 7, 7, 7));
    EXPECT_FALSE(zrt::all_equal(7, 7, 8, 7));
}

TEST(MiscTest, FuncWithRetrySucceedsFirstTry) {
    int called = 0;
    zrt::func_with_retry([&] { ++called; }, 3, "ok_func");
    EXPECT_EQ(called, 1);
}

TEST(MiscTest, FuncWithRetrySucceedsAfterRetry) {
    int called = 0;
    zrt::func_with_retry([&] {
        ++called;
        if (called < 2) throw std::runtime_error("transient");
    }, 3, "retry_func");
    EXPECT_EQ(called, 2);
}

TEST(MiscTest, FuncWithRetryThrowsAfterExhausted) {
    int called = 0;
    EXPECT_THROW(zrt::func_with_retry([&] {
        ++called;
        throw std::runtime_error("permanent");
    }, 2, "bad_func"), std::runtime_error);
    EXPECT_EQ(called, 2);
}

TEST(MiscTest, FuncWithRetryRejectsZeroRetries) {
    EXPECT_THROW(zrt::func_with_retry([] {}, 0, "x"), std::invalid_argument);
}

}  // namespace
