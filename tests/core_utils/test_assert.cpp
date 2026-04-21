#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "zrtools/zrt_assert.h"
#include "zrtools/zrt_exceptions.h"

namespace {

TEST(AssertTest, GAssertPassesWhenTrue) {
    EXPECT_NO_THROW({ g_assert(1 + 1 == 2, "math"); });
}

TEST(AssertTest, GAssertThrowsWhenFalse) {
    EXPECT_THROW({ g_assert(false, "should fire"); }, zrt::RunTimeError);
}

TEST(AssertTest, AssertEqPassesAndFails) {
    EXPECT_NO_THROW({ assert_eq(3, 3); });
    EXPECT_THROW({ assert_eq(3, 4); }, zrt::RunTimeError);
}

TEST(AssertTest, AssertStrEqWorks) {
    EXPECT_NO_THROW({ assert_str_eq("abc", std::string("abc")); });
    EXPECT_THROW({ assert_str_eq("abc", "abd"); }, zrt::RunTimeError);
}

TEST(AssertTest, AssertGtLtGe) {
    EXPECT_NO_THROW({ assert_gt(5, 3); });
    EXPECT_THROW({ assert_gt(3, 5); }, zrt::RunTimeError);

    EXPECT_NO_THROW({ assert_lt(3, 5); });
    EXPECT_THROW({ assert_lt(5, 3); }, zrt::RunTimeError);

    EXPECT_NO_THROW({ assert_ge(5, 5); });
    EXPECT_NO_THROW({ assert_ge(6, 5); });
    EXPECT_THROW({ assert_ge(4, 5); }, zrt::RunTimeError);
}

TEST(AssertTest, RunTimeErrorCarriesMessage) {
    try {
        throw zrt::RunTimeError("code={}", 42);
    } catch (const zrt::RunTimeError& e) {
        EXPECT_NE(std::string(e.what()).find("42"), std::string::npos);
    }
}

TEST(AssertTest, RunTimeErrorWithErrId) {
    try {
        throw zrt::RunTimeError(7, "msg {}", "x");
    } catch (const zrt::RunTimeError& e) {
        EXPECT_EQ(e.err_id(), 7);
        EXPECT_NE(std::string(e.what()).find("x"), std::string::npos);
    }
}

}  // namespace
