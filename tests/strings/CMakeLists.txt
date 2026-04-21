set(ZRT_STRINGS_TESTS
    test_zrt_string.cpp
    test_str_utils.cpp
    test_stl_dump.cpp
)

if (NOT (Boost_FOUND AND spdlog_FOUND))
    message(STATUS "strings: skipping tests (Boost_FOUND=${Boost_FOUND}, spdlog_FOUND=${spdlog_FOUND})")
    return()
endif()

foreach(src ${ZRT_STRINGS_TESTS})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE zrt_test_common)
    gtest_discover_tests(${name})
endforeach()
