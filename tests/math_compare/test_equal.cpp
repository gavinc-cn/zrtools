#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <string>

#include "zrtools/zrt_equal.h"

namespace {

enum class Kind : int { kA = 1, kB = 2 };

TEST(ZrtEqualTest, IntegersSameType) {
    EXPECT_TRUE(zrt::equal(5, 5));
    EXPECT_FALSE(zrt::equal(5, 6));
}

TEST(ZrtEqualTest, IntegersDifferentTypes) {
    EXPECT_TRUE(zrt::equal(5, 5U));
    EXPECT_TRUE(zrt::equal(5L, int64_t(5)));
}

TEST(ZrtEqualTest, CStrings) {
    EXPECT_TRUE(zrt::equal("abc", "abc"));
    EXPECT_FALSE(zrt::equal("abc", "abd"));
}

TEST(ZrtEqualTest, StringAndCString) {
    EXPECT_TRUE(zrt::equal(std::string("abc"), "abc"));
    EXPECT_TRUE(zrt::equal("abc", std::string("abc")));
}

TEST(ZrtEqualTest, CharAndString) {
    EXPECT_TRUE(zrt::equal('x', std::string("x")));
    EXPECT_TRUE(zrt::equal(std::string("x"), 'x'));
    EXPECT_FALSE(zrt::equal('x', std::string("y")));
}

TEST(ZrtEqualTest, CStringAndIntUsingStringification) {
    EXPECT_TRUE(zrt::equal("123", 123));
    EXPECT_TRUE(zrt::equal(123, "123"));
    EXPECT_FALSE(zrt::equal("123", 124));
}

TEST(ZrtEqualTest, DoubleNaNSafe) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(zrt::equal(nan, nan));
    EXPECT_FALSE(zrt::equal(nan, 1.0));
    EXPECT_TRUE(zrt::equal(1.0, 1.0 + 1e-12));         // epsilon 内相等
    EXPECT_FALSE(zrt::equal(1.0, 1.01));
}

TEST(ZrtEqualTest, FloatNaNSafe) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(zrt::equal(nan, nan));
    EXPECT_TRUE(zrt::equal(1.0f, 1.0f + 1e-8f));
}

TEST(ZrtEqualTest, EnumEqualsInt) {
    EXPECT_TRUE(zrt::equal(1, Kind::kA));
    EXPECT_TRUE(zrt::equal(Kind::kB, 2));
    EXPECT_FALSE(zrt::equal(Kind::kA, 2));
}

}  // namespace
