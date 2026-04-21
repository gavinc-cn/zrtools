#include <gtest/gtest.h>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "zrtools/stl_dump.h"

namespace {

TEST(StlDumpTest, ToStrInteger) {
    EXPECT_EQ(zrt::to_str(42), "42");
}

TEST(StlDumpTest, ToStrDoubleUsesEightDigitPrecision) {
    // setprecision(8) 是总有效数字, 不是小数位
    EXPECT_EQ(zrt::to_str(3.14159265), "3.1415927");
}

TEST(StlDumpTest, VectorFormat) {
    std::vector<int> v{1, 2, 3};
    EXPECT_EQ(zrt::to_str(v), "[1, 2, 3]");

    std::vector<int> empty;
    EXPECT_EQ(zrt::to_str(empty), "[]");
}

TEST(StlDumpTest, MapFormat) {
    std::map<std::string, int> m{{"a", 1}, {"b", 2}};
    // map 有序, 输出按 key 排序
    EXPECT_EQ(zrt::to_str(m), "{a:1, b:2}");
}

TEST(StlDumpTest, PairFormat) {
    std::pair<int, std::string> p{7, "x"};
    EXPECT_EQ(zrt::to_str(p), "7:x");
}

TEST(StlDumpTest, TupleFormat) {
    auto t = std::make_tuple(1, std::string("hi"), 2.5);
    const std::string s = zrt::to_str(t);
    // 精度 setprecision(8), 2.5 原样输出
    EXPECT_EQ(s, "(1, hi, 2.5)");
}

}  // namespace
