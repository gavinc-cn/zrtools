# io_pool_v2 单元测试
#
# 约定：一个 test_*.cpp 对应 io_pool_v2 中的一个组件。
# 所有测试用例走 gtest_discover_tests 自动注册到 CTest。

# 纯 std 的测试：任何环境都能跑
set(ZRT_IO_POOL_V2_TESTS_STD
    test_lock_deque.cpp
)

# 需要 boost + spdlog 的测试
set(ZRT_IO_POOL_V2_TESTS_FULL
    test_sync_thread.cpp
    # TODO: 等骨架跑通后再补实现
    # test_boost_asio_thread.cpp
    # test_lock_deque_thread.cpp
    # test_boost_lockfree_thread.cpp
    # test_moodycamel_thread.cpp
    # test_engine_pool.cpp
    # test_i_handler.cpp
    # test_io_service.cpp
    # test_io_pool.cpp
)

foreach(src ${ZRT_IO_POOL_V2_TESTS_STD})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE zrt_test_common)
    gtest_discover_tests(${name})
endforeach()

if (Boost_FOUND AND spdlog_FOUND)
    foreach(src ${ZRT_IO_POOL_V2_TESTS_FULL})
        get_filename_component(name ${src} NAME_WE)
        add_executable(${name} ${src})
        target_link_libraries(${name} PRIVATE zrt_test_common)
        gtest_discover_tests(${name})
    endforeach()
else()
    message(STATUS "io_pool_v2: skipping boost/spdlog-dependent tests "
                   "(Boost_FOUND=${Boost_FOUND}, spdlog_FOUND=${spdlog_FOUND})")
endif()
