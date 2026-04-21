# zrtools 文档索引

`zrtools` 是一组 C++17 通用工具库，集中在 `src/zrtools/` 下，以头文件为主（含少量 `.cpp`）。
库整体无强制链接依赖，模块按需引入 `boost`、`spdlog`、`fmt` 等第三方库。

源码根：`src/zrtools/`（`CMakeLists.txt` 已将其加入 `include` 路径，使用 `#include "zrtools/xxx.h"` 引入）。

## 模块索引

| 分组 | 文档 | 涉及文件 |
|------|------|----------|
| 核心工具 | [core_utils.md](core_utils.md) | `zrt_define.h`、`zrt_cpp14_compat.h`、`zrt_type_traits.h`、`zrt_assert.h`、`zrt_exceptions.h`、`zrt_misc.h`、`misc.h`、`fmt_helper.h` |
| 字符串 | [strings.md](strings.md) | `zrt_string.h`、`zrt_string_view.h`、`zrt_string_keys.h`、`str_utils.h`、`stl_dump.h` |
| 数值 / 比较 / 填充 | [math_compare.md](math_compare.md) | `zrt_math.h`、`zrt_compare.h`、`zrt_equal.h`、`zrt_fill.h` |
| CRC32 / Snowflake | [crc32_snowflake.md](crc32_snowflake.md) | `zrt_crc32.h`、`zrt_snowflake.h` |
| 时间 | [time.md](time.md) | `zrt_time.h`、`zrt_time-inl.h`、`zrt_time.cpp`、`time_buffer.h`、`kronos.h` |
| 文件 | [file.md](file.md) | `file.h/.cpp`、`cfile.h/.cpp`、`zrt_file.h`、`filesystem_compatible.h` |
| 并发 | [concurrency.md](concurrency.md) | `thread.h`、`mutex.h`、`zrt_locked_hash_map.h` |
| 缓冲 / 队列 | [buffers.md](buffers.md) | `ultrabuf.h/.cpp`、`ultraq.h` |
| Boost MultiIndex 外观 | [bmic.md](bmic.md) | `zrt_bmic.h`、`zrt_bmic_hashed.h`、`zrt_bmic_ordered.h` |
| 共享内存 | [shm.md](shm.md) | `zrt_shm.h`、`zrt_shm_direct.h`、`zrt_shm_ring.h`、`zrt_shm_checkpoint.h` |
| WAL 持久化 | [wal.md](wal.md) | `zrt_wal_entry.h`、`zrt_wal_file.h` |
| 布林带指标 | [bollinger.md](bollinger.md) | `bollinger.h` |
| IO 线程池 v2 | [io_pool_v2.md](io_pool_v2.md) | `io_pool_v2/*` |
| Python 代码生成 | [py_scripts.md](py_scripts.md) | `py_scripts/*.py` |

## 构建

顶层 CMake 片段：

```
cmake_minimum_required(VERSION 3.14)
project(zrtools CXX)
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(src/zrtools)
if (ZRT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

测试使用 `FetchContent` 拉取 GoogleTest v1.14.0，按模块分目录放在 `tests/<module>/`，`gtest_discover_tests` 自动注册到 CTest。
需要 `boost` / `spdlog` 的测试会在 `find_package` 找不到时被自动跳过。

## 命名约定

- 命名空间：除少量宏与 `Bollinger` 等历史类外，新代码基本放在 `namespace zrt` 下。
- 头文件前缀：旧版直接叫 `misc.h`、`file.h`、`cfile.h` 等；新版加 `zrt_` 前缀（`zrt_misc.h`、`zrt_file.h`），并往往是旧版的增强替代。
- `ZRT_` 前缀的宏多为可开关/声明宏（`ZRT_LIKELY`、`ZRT_DECLARE_SINGLETON`、`ZRT_BMIC` 等）。
- 内部模板类常用 PascalCase（`DateTimeUTC`、`LockedHashMap`、`ShmRing`），自由函数偏 snake_case（`get_uuid`、`safe_div`）。两者混用属遗留风格。

## 状态

- 所有文档为重建/整理版，贴合当前源码现状。
- 新迁入的项目还未全部拉起单元测试；已上线 `io_pool_v2/test_lock_deque.cpp`、`io_pool_v2/test_sync_thread.cpp`，其余按本目录说明逐步补齐。
