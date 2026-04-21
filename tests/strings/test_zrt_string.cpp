#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "zrtools/zrt_string.h"

namespace {

TEST(ZrtStringTest, ConvertStringToInt) {
    EXPECT_EQ(zrt::convert<int>(std::string("42")), 42);
    EXPECT_EQ(zrt::convert<int64_t>(std::string("-100")), -100);
}

TEST(ZrtStringTest, ConvertStringToDouble) {
    EXPECT_DOUBLE_EQ(zrt::convert<double>(std::string("3.14")), 3.14);
}

TEST(ZrtStringTest, ConvertBadInputReturnsDefault) {
    // 失败时走 lexical_cast 异常 → 返回 default_val
    EXPECT_EQ(zrt::convert<int>(std::string("abc"), -1), -1);
}

TEST(ZrtStringTest, ConvertEmptyReturnsValueInitialized) {
    EXPECT_EQ(zrt::convert<int>(std::string("")), 0);
    EXPECT_EQ(zrt::convert<std::string>(std::string("")), std::string(""));
}

TEST(ZrtStringTest, Convert2StrInt) {
    EXPECT_EQ(zrt::convert2str(42), "42");
    EXPECT_EQ(zrt::convert2str(-7), "-7");
}

TEST(ZrtStringTest, Convert2StrDoubleWithPrecision) {
    EXPECT_EQ(zrt::convert2str(3.14159, 2), "3.14");
    EXPECT_EQ(zrt::convert2str(0.0, 3), "0.000");
    EXPECT_EQ(zrt::convert2str(-0.0, 3), "0.000");  // 脚注：-0 修正为 0
}

TEST(ZrtStringTest, Convertible2IntAndAllConvertible) {
    EXPECT_TRUE(zrt::convertible2int("123"));
    EXPECT_FALSE(zrt::convertible2int("abc"));

    std::vector<std::string> ok{"1", "2", "3"};
    std::vector<std::string> bad{"1", "x", "3"};
    EXPECT_TRUE(zrt::all_convertible2int(ok));
    EXPECT_FALSE(zrt::all_convertible2int(bad));
}

TEST(ZrtStringTest, ToLowerUpper) {
    EXPECT_EQ(zrt::to_lower("Hello WORLD"), "hello world");
    EXPECT_EQ(zrt::to_upper("Hello WORLD"), "HELLO WORLD");
    EXPECT_EQ(zrt::to_lower(""), "");
}

TEST(ZrtStringTest, Contain) {
    EXPECT_TRUE(zrt::contain("hello world", "world"));
    EXPECT_TRUE(zrt::contain("hello", ""));  // 空串永远包含
    EXPECT_FALSE(zrt::contain("hello", "xyz"));
}

}  // namespace
