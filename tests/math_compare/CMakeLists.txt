set(ZRT_MATH_COMPARE_TESTS
    test_math.cpp
    test_compare.cpp
    test_equal.cpp
    test_fill.cpp
)

if (NOT (Boost_FOUND AND spdlog_FOUND))
    message(STATUS "math_compare: skipping tests (Boost_FOUND=${Boost_FOUND}, spdlog_FOUND=${spdlog_FOUND})")
    return()
endif()

foreach(src ${ZRT_MATH_COMPARE_TESTS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE zrt_test_common)
    gtest_discover_tests(${name})
endforeach()
