#include <gtest/gtest.h>
#include <cmath>

#include "zrtools/zrt_math.h"

namespace {

TEST(ZrtMathTest, RoundToNDecimals) {
    EXPECT_DOUBLE_EQ(zrt::round(3.14159, 2), 3.14);
    EXPECT_DOUBLE_EQ(zrt::round(3.145, 2), 3.15);     // 正常四舍五入
    EXPECT_DOUBLE_EQ(zrt::round(-3.14159, 2), -3.14);
}

TEST(ZrtMathTest, RoundToPrecision) {
    EXPECT_DOUBLE_EQ(zrt::round_to(10.27, 0.05), 10.25);
    EXPECT_DOUBLE_EQ(zrt::round_to(10.26, 0.05), 10.25);
    EXPECT_DOUBLE_EQ(zrt::round_to(10.28, 0.05), 10.30);
}

TEST(ZrtMathTest, RoundToZeroPrecisionReturnsOriginal) {
    EXPECT_DOUBLE_EQ(zrt::round_to(10.0, 0.0), 10.0);
}

TEST(ZrtMathTest, FloorTo) {
    EXPECT_DOUBLE_EQ(zrt::floor_to(10.29, 0.05), 10.25);
    EXPECT_DOUBLE_EQ(zrt::floor_to(10.30, 0.05), 10.30);
}

TEST(ZrtMathTest, GetDecimalPlaces) {
    EXPECT_EQ(zrt::get_decimal_places(0.1), 1);
    EXPECT_EQ(zrt::get_decimal_places(0.01), 2);
    EXPECT_EQ(zrt::get_decimal_places(0.0001), 4);
    EXPECT_EQ(zrt::get_decimal_places(1.0), 0);
    EXPECT_EQ(zrt::get_decimal_places(0.0), 0);
}

TEST(ZrtMathTest, RoundStr) {
    EXPECT_EQ(zrt::round_str(3.14159, 2), "3.14");
    EXPECT_EQ(zrt::round_str(1.0, 3), "1.000");
}

TEST(ZrtMathTest, SMAOverFullWindow) {
    zrt::SMA<double> sma(3);
    sma.Add(1.0);
    sma.Add(2.0);
    sma.Add(3.0);
    EXPECT_DOUBLE_EQ(sma.GetAvg(), 2.0);
    EXPECT_EQ(sma.GetSize(), 3U);

    sma.Add(4.0);  // 窗口滚动: {2,3,4}
    EXPECT_DOUBLE_EQ(sma.GetAvg(), 3.0);
    EXPECT_EQ(sma.GetSize(), 3U);
}

TEST(ZrtMathTest, SMAUnderFullWindow) {
    zrt::SMA<double> sma(5);
    sma.Add(10.0);
    EXPECT_DOUBLE_EQ(sma.GetAvg(), 10.0);
    EXPECT_EQ(sma.GetSize(), 1U);
    sma.Add(20.0);
    EXPECT_DOUBLE_EQ(sma.GetAvg(), 15.0);
    EXPECT_EQ(sma.GetSize(), 2U);
}

}  // namespace
