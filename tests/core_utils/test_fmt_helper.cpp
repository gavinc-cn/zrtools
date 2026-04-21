#include <gtest/gtest.h>
#include <string>
#include <type_traits>

#include "zrtools/fmt_helper.h"

namespace {

TEST(FmtHelperTest, SafeFormatValueIntegral) {
    const auto v = zrt::safe_format_value(42);
    static_assert(std::is_same<std::decay_t<decltype(v)>, int>::value,
                  "integral should stay int");
    EXPECT_EQ(v, 42);
}

TEST(FmtHelperTest, SafeFormatValueNonNullChar) {
    const auto v = zrt::safe_format_value('A');
    EXPECT_EQ(v, std::string("A"));
}

TEST(FmtHelperTest, SafeFormatValueNullCharBecomesEmpty) {
    const auto v = zrt::safe_format_value('\0');
    EXPECT_EQ(v, std::string{});
}

TEST(FmtHelperTest, SafeFmtBasicFormatting) {
    EXPECT_EQ(zrt::safe_fmt("x={}, y={}", 1, 2), "x=1, y=2");
    EXPECT_EQ(zrt::safe_fmt("{:.2f}", 3.14159), "3.14");
}

TEST(FmtHelperTest, SafeFmtCatchesFormatException) {
    // 模板参数太多或 arg 过少会在 fmt 里抛异常，safe_fmt 应降级为错误字符串
    const std::string s = zrt::safe_fmt("{} {}", 1);  // 只给一个 arg
    EXPECT_NE(s.find("fmt::format error"), std::string::npos);
}

}  // namespace
