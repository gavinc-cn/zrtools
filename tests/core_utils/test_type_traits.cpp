#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <list>

#include "zrtools/zrt_type_traits.h"

namespace {

TEST(TypeTraitsTest, IsCStrRecognizesArraysAndPointers) {
    EXPECT_TRUE(zrt::IsCStr_v<char[10]>);
    EXPECT_TRUE(zrt::IsCStr_v<const char[10]>);
    EXPECT_TRUE(zrt::IsCStr_v<char*>);
    EXPECT_TRUE(zrt::IsCStr_v<const char*>);
    EXPECT_FALSE(zrt::IsCStr_v<std::string>);
    EXPECT_FALSE(zrt::IsCStr_v<int*>);
    EXPECT_FALSE(zrt::IsCStr_v<int[3]>);
}

TEST(TypeTraitsTest, IsIntegralNumExcludesChar) {
    EXPECT_TRUE(zrt::IsIntegralNum_v<int>);
    EXPECT_TRUE(zrt::IsIntegralNum_v<int64_t>);
    EXPECT_TRUE(zrt::IsIntegralNum_v<bool>);      // bool 也算
    EXPECT_FALSE(zrt::IsIntegralNum_v<char>);     // char 明确排除
    EXPECT_FALSE(zrt::IsIntegralNum_v<double>);
    EXPECT_FALSE(zrt::IsIntegralNum_v<std::string>);
}

TEST(TypeTraitsTest, IsIntNumExcludesCharAndBool) {
    EXPECT_TRUE(zrt::IsIntNum_v<int>);
    EXPECT_TRUE(zrt::IsIntNum_v<uint32_t>);
    EXPECT_FALSE(zrt::IsIntNum_v<char>);
    EXPECT_FALSE(zrt::IsIntNum_v<bool>);
}

TEST(TypeTraitsTest, IsFloatingPoint) {
    EXPECT_TRUE(zrt::IsFloatingPoint_v<float>);
    EXPECT_TRUE(zrt::IsFloatingPoint_v<double>);
    EXPECT_FALSE(zrt::IsFloatingPoint_v<int>);
}

TEST(TypeTraitsTest, IsPair) {
    EXPECT_TRUE((zrt::is_pair<std::pair<int, std::string>>::value));
    EXPECT_FALSE(zrt::is_pair<int>::value);
    EXPECT_FALSE(zrt::is_pair<std::string>::value);
}

TEST(TypeTraitsTest, IsIterableRecognizesContainers) {
    EXPECT_TRUE(zrt::is_iterable<std::vector<int>>::value);
    EXPECT_TRUE(zrt::is_iterable<std::list<double>>::value);
    EXPECT_TRUE(zrt::is_iterable<std::string>::value);
    EXPECT_FALSE(zrt::is_iterable<int>::value);
    EXPECT_FALSE(zrt::is_iterable<double>::value);
}

TEST(TypeTraitsTest, IsBothIntegralNumRequiresBoth) {
    EXPECT_TRUE((zrt::IsBothIntegralNum_v<int, long>));
    EXPECT_FALSE((zrt::IsBothIntegralNum_v<int, double>));
    EXPECT_FALSE((zrt::IsBothIntegralNum_v<int, char>));
}

}  // namespace
