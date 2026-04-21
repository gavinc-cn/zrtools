# 核心工具模块

通用宏、类型萃取、断言、异常、杂项工具。大多是头文件内联实现。

## zrt_define.h

项目根宏定义。

| 宏 | 说明 |
|----|------|
| `ZRT_LIKELY(x)` / `ZRT_UNLIKELY(x)` | GCC/Clang 下 `__builtin_expect` 分支提示，MSVC 退化为 `(x)`。 |
| `ZRT_DECLARE_SINGLETON(cls)` | 类内声明单例。自动生成 `GetInstance()`、屏蔽拷贝构造/赋值，并 `private` 声明 `cls()`、`~cls()`，使用者需自己实现。 |
| `ZRT_VAR2STR(var)` | 同 `#var`，把变量名字面量化为字符串。 |

## zrt_cpp14_compat.h

C++14/17 兼容层。

- `INLINE_VAR`：C++17 下为 `inline`，C++14 下展开为空，便于头文件中定义全局 `constexpr` 变量。
- Windows 下补齐 `uchar` / `ushort` / `uint` / `ulong` typedef。
- `zrt::StrView`：C++17 使用 `std::string_view`，否则回退到 `boost::string_view`。

## zrt_type_traits.h

所有 trait 位于 `namespace zrt`。内部使用 `void_t` SFINAE。

| Trait | 含义 |
|-------|------|
| `is_pair<T>` | `T` 是 `std::pair<X,Y>` 的特化 |
| `has_iterator<T>` | `T::iterator` 存在 |
| `has_begin_end<T>` | `std::begin/std::end` 可用 |
| `is_iterable<T>` | `has_begin_end && has_iterator` |
| `IsCStr<T>` / `IsCStr_v<T>` | `char[N]` / `char*` / `const char*` |
| `IsChar_v<T>` | 刚好是 `char` |
| `IsIntegralNum<T>` / `IsIntegralNum_v<T>` | 整数（不含 `char`，**包含 `bool`**） |
| `IsIntNum<T>` / `IsIntNum_v<T>` | 整数（不含 `char`、`bool`） |
| `IsFloatingPoint_v<T>` | `std::is_floating_point` |
| `IsPointer_v<T>` / `IsEnum_v<T>` | 同 `std::is_pointer` / `std::is_enum` |
| `IsBothIntegralNum_v<T,U>` | 两参数都是 `IsIntegralNum_v` |

## zrt_assert.h

基于 `spdlog` + `zrt::RunTimeError` 的运行时断言宏。**断言失败会抛异常**，不适合硬中断。

| 宏 | 行为 |
|----|------|
| `g_assert(expr, message)` | 失败时 `SPDLOG_ERROR` 并抛 `zrt::RunTimeError` |
| `assert_eq(a,b)` / `assert_str_eq(a,b)` | `a == b` / `std::string(a) == std::string(b)` |
| `assert_gt/lt/ge(a,b)` | 大于 / 小于 / 大于等于 |
| `ZRT_EXPECT(logger, expr, msg)` | 失败只记日志，不抛；`logger` 是形如 `SPDLOG_WARN` 的宏 |
| `ZRT_EXPECT_EQ(logger, a, b)` | 同上，判等用 `zrt::equal` |

## zrt_exceptions.h

- `zrt::RunTimeError : std::runtime_error`：模板构造可直接 `fmt::format` 拼错误信息，支持带 `int err_id` 的重载，`err_id()` 取回。
- `zrt::TypeConvertError<T,U>`：类型转换失败；`what()` 返回 `type X with val V cannot be converted to U`。
- `zrt::safe_terminate_hdl()`：可作为 `std::set_terminate` 的处理器。会尝试拉取当前异常、打印 stacktrace、`spdlog::shutdown` 后 `abort`。使用原子位防重入二次 abort。
- `zrt::simple_terminate_hdl()`：精简版，不依赖 `SAFE_TERMINATE_LOG` 的 fallback 逻辑。
- 辅助宏 `SPDLOG_ERROR_STACK_TRACE(...)`：在错误日志里附 stacktrace。
- 辅助宏 `SAFE_TERMINATE_LOG(prefix, msg)`：无 logger 时回退到 `std::cerr`。

> `safe_terminate_hdl` 依赖 `boost::stacktrace`，Linux 构建时需链接 `boost_stacktrace_backtrace` 或 `-lboost_stacktrace_basic`，否则符号缺失。

## zrt_misc.h

集中收敛随机、UUID、MD5、map 取值、可变参数判等、堆范围、重试等小工具。**全部 `namespace zrt`**。

### 随机与标识
- `T get_random(T low, T high)` / `T get_random()`：`thread_local` 引擎 + 均匀分布。
  注意：`distribution` 也是 `thread_local` 且固定边界，后续调用不同 `low/high` 会复用首个分布 → 首次调用即确定范围。
- `std::string get_uuid()`：`thread_local` 的 `boost::uuids::random_generator`。
- `std::string get_md5(const char* buffer, size_t buffer_size)`：hex 形式的 MD5。

### 数值/容器
- `double safe_div(a, b, default_val = NAN)`：`b==0` 时返回 `default_val`。
- `bool get_map_val(const unordered_map<K,V>&, const K&, V&)`：取不到时记日志并返回 `false`。
- `Val GetMapVal<Val,Key,Map>(map, key, default_v = {})`：取不到返回默认值。
- `Val GetUmapVal<...>(const unordered_map<K,V>&, key, default_v = {})`：`GetMapVal` 对 `unordered_map` 的特化重载。
- `bool TryGetMapVal(map, key, Val& out)`。
- `ZRT_GET_MAP_VAL(map,key,val)`：宏版本，失败时 `SPDLOG_ERROR`。

### 逻辑
- `equal_any_of(target, a, b, ...)`：target 是否等于任意后续参数（走 `zrt::equal`）。
- `all_equal(target, a, b, ...)`：target 是否等于所有后续参数。

### 哈希
- `TupleHasher` / `PairHasher`：`operator()` 对 `std::tuple<...>` / `std::pair<...>` 取 `boost::hash_combine` 组合哈希。
- `PrimeKey<T> = decltype(GetPKey(std::declval<T>()))`：约定业务方提供 `GetPKey()` ADL 重载。
- `TupleHashMap<T> = std::unordered_map<PrimeKey<T>, T, TupleHasher>`。

### 其他
- `HeapRange get_heap_range()`（Linux）：解析 `/proc/self/maps` 的 `[heap]` 行，返回堆起止与大小。
- `std::string get_hostname()`（仅 Linux）：`gethostname` 封装。
- `func_with_retry(func, retry_time, func_name)`：最多重试 N 次，最终仍失败重新抛出 `std::runtime_error`。`retry_time <= 0` 会抛 `std::invalid_argument`。
- 宏 `ZYIR_FIELD_FILTER_CTN/RET`：业务筛选宏，与 `zrt::is_empty` / `zrt::equal` 配合。
- 宏 `DECLARE_KEY(key)` / `DECLARE_K(key)`：在头文件声明 `static const char K_name[] = "name"` / `k_name[] = "name"`。

## misc.h

`zrt_misc.h` 的早期版本。只有 `get_random` / `get_uuid` / `get_md5`。
内部使用 `static`（非 `thread_local`）随机引擎，**无线程安全保证**。仅保留是为了兼容旧代码，新代码一律用 `zrt_misc.h`。

> 另一处同名宏 `ZRT_VAR2STR(A)` 也在此处定义，与 `zrt_define.h` 重复。同时包含两者可能触发重定义告警，业务中二选一。

## fmt_helper.h

所有函数在 `namespace zrt`。

- `safe_format_value(T&&)`：C++17 `if constexpr`，把 `'\0'` 映射为空字符串，其他类型原样 `std::forward`。用于给 `fmt::format` 喂参数，避免直接把空 char 当空串占位。
- `print_fmt(Args&&...)`：`std::cout << fmt::format(...)` 封装，末尾补 `\n`。
- `safe_fmt(Args&&...)`：吞掉 `fmt::format` 异常，返回 `"fmt::format error: ..."` 字符串，避免格式串错误导致进程崩溃。

## 典型用法

```cpp
#include "zrtools/zrt_misc.h"
#include "zrtools/zrt_exceptions.h"

const auto id = zrt::get_uuid();
const auto timeout_ms = zrt::GetMapVal<int>(cfg, "timeout_ms", 1000);

if (timeout_ms <= 0) {
    throw zrt::RunTimeError("invalid timeout_ms: {}", timeout_ms);
}

zrt::func_with_retry([&]{ do_network_call(); }, 3, "network_call");
```
