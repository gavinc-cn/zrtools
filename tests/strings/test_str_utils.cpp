#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "zrtools/str_utils.h"

namespace {

TEST(StrUtilsTest, SplitByChar) {
    const auto v = zrt::split("a,b,c", ',');
    ASSERT_EQ(v.size(), 3U);
    EXPECT_EQ(v[0], "a");
    EXPECT_EQ(v[1], "b");
    EXPECT_EQ(v[2], "c");
}

TEST(StrUtilsTest, SplitTrailingDelimiterProducesEmpty) {
    const auto v = zrt::split("a,b,", ',');
    ASSERT_EQ(v.size(), 3U);
    EXPECT_EQ(v[0], "a");
    EXPECT_EQ(v[1], "b");
    EXPECT_EQ(v[2], "");
}

TEST(StrUtilsTest, SplitEmptyInput) {
    const auto v = zrt::split("", ',');
    EXPECT_TRUE(v.empty());
}

TEST(StrUtilsTest, SplitByRegexStringDelimiter) {
    const auto v = zrt::split(std::string("a::b::c"), std::string("::"));
    ASSERT_EQ(v.size(), 3U);
    EXPECT_EQ(v[0], "a");
    EXPECT_EQ(v[1], "b");
    EXPECT_EQ(v[2], "c");
}

TEST(StrUtilsTest, JoinVector) {
    EXPECT_EQ(zrt::join({"a", "b", "c"}, ','), "a,b,c");
    EXPECT_EQ(zrt::join({"solo"}, ','), "solo");
    EXPECT_EQ(zrt::join({}, ','), "");
}

TEST(StrUtilsTest, LowerUpperMutateInPlace) {
    std::string s = "Hello";
    const auto ref = zrt::lower(s);
    EXPECT_EQ(s, "hello");       // 原地改
    EXPECT_EQ(ref, "hello");

    s = "world";
    zrt::upper(s);
    EXPECT_EQ(s, "WORLD");
}

}  // namespace
