#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <string>

#include "zrtools/zrt_compare.h"

namespace {

TEST(ZrtCompareTest, GreaterIntegers) {
    EXPECT_TRUE(zrt::greater(5, 3));
    EXPECT_FALSE(zrt::greater(3, 3));
    EXPECT_FALSE(zrt::greater(2, 3));
}

TEST(ZrtCompareTest, GreaterDoublesRespectEpsilon) {
    // 相差在 epsilon 内 → 视为相等 → greater 返回 false
    EXPECT_FALSE(zrt::greater(1.0, 1.0 + 1e-10));
    EXPECT_TRUE(zrt::greater(1.0, 0.99));
}

TEST(ZrtCompareTest, LessStrings) {
    EXPECT_TRUE(zrt::less(std::string("aaa"), "aab"));
    EXPECT_FALSE(zrt::less(std::string("aab"), "aaa"));
}

TEST(ZrtCompareTest, GreaterEqualLessEqual) {
    EXPECT_TRUE(zrt::greater_equal(5, 5));
    EXPECT_TRUE(zrt::greater_equal(5, 3));
    EXPECT_FALSE(zrt::greater_equal(3, 5));

    EXPECT_TRUE(zrt::less_equal(3.0, 3.0 + 1e-10));
    EXPECT_TRUE(zrt::less_equal(3.0, 4.0));
    EXPECT_FALSE(zrt::less_equal(4.0, 3.0));
}

TEST(ZrtCompareTest, RelativeCloseHandlesNaN) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(zrt::relative_close(nan, nan));
    EXPECT_FALSE(zrt::relative_close(nan, 1.0));
}

TEST(ZrtCompareTest, RelativeCloseNearValues) {
    EXPECT_TRUE(zrt::relative_close(1000.0, 1000.0 + 1e-9));
    EXPECT_FALSE(zrt::relative_close(1000.0, 1001.0));
}

TEST(ZrtCompareTest, IsEmpty) {
    EXPECT_TRUE(zrt::is_empty(0));
    EXPECT_FALSE(zrt::is_empty(1));
    EXPECT_TRUE(zrt::is_empty(0.0));
    EXPECT_FALSE(zrt::is_empty(0.5));
    EXPECT_TRUE(zrt::is_empty(std::string{}));
    EXPECT_FALSE(zrt::is_empty(std::string("x")));
    EXPECT_TRUE(zrt::is_empty(""));
    EXPECT_FALSE(zrt::is_empty("x"));
}

TEST(ZrtCompareTest, VariadicMaxMin) {
    EXPECT_EQ(zrt::max(1, 2, 3, 4), 4);
    EXPECT_EQ(zrt::min(1, 2, 3, 4), 1);
    EXPECT_EQ(zrt::max(-1, -5, -3), -1);
}

TEST(ZrtCompareTest, VariadicMaxMinNaNAware) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // NaN 应被跳过
    EXPECT_DOUBLE_EQ(zrt::max(1.0, nan, 2.0, 0.5), 2.0);
    EXPECT_DOUBLE_EQ(zrt::min(1.0, nan, 2.0, 0.5), 0.5);
}

TEST(ZrtCompareTest, MyLessFunctor) {
    zrt::my_less<int> less_i;
    EXPECT_TRUE(less_i(1, 2));
    EXPECT_FALSE(less_i(2, 1));
}

TEST(ZrtCompareTest, MyEqualFunctor) {
    zrt::my_equal<int> eq_i;
    EXPECT_TRUE(eq_i(5, 5));
    EXPECT_FALSE(eq_i(5, 6));
}

}  // namespace
