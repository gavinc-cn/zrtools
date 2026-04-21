include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)

# Windows: 防止 gtest 覆盖父项目的编译器/链接器设置
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# 可选：外部依赖（io_pool_v2 的部分实现需要）
# 这些 find_package 在未安装时不会 fatal —— 对应测试会被跳过。
find_package(Boost COMPONENTS system thread)
find_package(spdlog QUIET)

# 公共测试目标配置：凡 link 本库即可获得 include path / 依赖
add_library(zrt_test_common INTERFACE)
target_include_directories(zrt_test_common INTERFACE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(zrt_test_common INTERFACE
    GTest::gtest GTest::gtest_main
    zrtools              # 链接 src/zrtools 下所有 .cpp 的静态库
)
if (Boost_FOUND)
    target_link_libraries(zrt_test_common INTERFACE Boost::system Boost::thread)
endif()
if (spdlog_FOUND)
    target_link_libraries(zrt_test_common INTERFACE spdlog::spdlog)
endif()

# io_pool_v2 已有的骨架
add_subdirectory(io_pool_v2)

# 按模块分目录，每个目录里是独立的 test_*.cpp
add_subdirectory(core_utils)
add_subdirectory(strings)
add_subdirectory(math_compare)
add_subdirectory(crc32_snowflake)
add_subdirectory(time)
add_subdirectory(file)
add_subdirectory(concurrency)
add_subdirectory(buffers)
add_subdirectory(bmic)
add_subdirectory(shm)
add_subdirectory(wal)
add_subdirectory(bollinger)
