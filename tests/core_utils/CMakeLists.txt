# core_utils 模块测试
#
# 约定：一个 test_*.cpp 一个可执行，挂到 zrt_test_common 上。

set(ZRT_CORE_UTILS_TESTS
    test_type_traits.cpp
    test_misc.cpp
    test_fmt_helper.cpp
    test_assert.cpp
)

if (NOT (Boost_FOUND AND spdlog_FOUND))
    message(STATUS "core_utils: skipping tests (Boost_FOUND=${Boost_FOUND}, spdlog_FOUND=${spdlog_FOUND})")
    return()
endif()

foreach(src ${ZRT_CORE_UTILS_TESTS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE zrt_test_common)
    gtest_discover_tests(${name})
endforeach()
