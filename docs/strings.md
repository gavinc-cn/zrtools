# 字符串 & 打印模块

收敛字符串转换、大小写、切分/连接、容器打印等。

## zrt_string.h

所有函数位于 `namespace zrt`；底层用 `boost::lexical_cast`。

| API | 说明 |
|-----|------|
| `ReturnType convert<ReturnType,InputType>(const InputType&, const ReturnType& default_val = {})` | 失败记日志并返回 `default_val`；`zrt::is_empty(input)` 返回值类型的 `{}`。 |
| `std::string convert2str(T)` | `std::to_string` 透传。 |
| `std::string convert2str(double, int precision)` | 定宽小数；特判 `-0.000…` 映射回 `0.000…`。 |
| `bool convertible2int(const std::string&)` | `std::stoi` 包一层 try/catch。 |
| `bool all_convertible2int(const std::vector<std::string>&)` | 集合版。 |
| `std::string to_lower/to_upper(const std::string&)` | 返回新串（ASCII `std::tolower/toupper`）。 |
| `bool contain(str, sub)` | `find != npos`。 |

辅助枚举 `FAIL_OPERATION { EM_THROW, EM_RETURN_DEFAULT, EM_CUSTOM_HANDLER }` 目前只是占位；宏 `DEFAULT_FAIL_OPERATION=0` 同理。

## zrt_string_view.h & zrt_cpp14_compat.h（StrView）

两个文件都定义 `zrt::StrView`：C++17 为 `std::string_view`，否则 `boost::string_view`。同一工程内只需包含其一。

## zrt_string_keys.h

一组字面量常量名，由 `DECLARE_K(key)` 展开为 `static const char k_key[] = "key"`，避免散落在各业务处的魔法字符串：

```
k_trace, k_debug, k_info, k_warn, k_error, k_critical, k_sync_logger
```

它 include 了 `zrt_misc.h`（内含宏定义），所以是**重量级包含**，若只为了常量字符串应考虑单独提取。

## str_utils.h

命名空间 `zrt`，底层使用 `boost::algorithm` + `boost::regex`。

| API | 语义 |
|-----|------|
| `std::vector<std::string> split(const std::string& s, char delim)` | 基于 `std::getline`。若 `s` 以 `delim` 结尾，会额外补一个空串元素——匹配 Python `str.split` 行为。 |
| `std::vector<std::string> split(const std::string& s, const std::string& delim_regex)` | `boost::split_regex`，delim 视为正则。 |
| `std::string join(vec, char delim)` | 常规拼接。 |
| `std::string join(vec, char delim, size_t begin, size_t end)` | `begin..end)` 左闭右开，越界时截到 `v.size()`。 |
| `std::string lower/upper(std::string& s)` | **原地修改并返回引用**，不同于 `zrt_string.h::to_lower/to_upper`（后者返回新串）。 |

## stl_dump.h

为常见 STL 容器重载 `operator<<`，便于 `spdlog::info("{}", vec)` 或 `std::cout << map`。

**注意：这些 `operator<<` 定义在全局命名空间**（宏生成，不加任何命名空间包裹）。include 后会影响全局流输出；某些 IDE/warning 检查会提示 "overload in global scope"。

### 支持的类型
- 单元素（`[a, b, c]`）：`vector`、`set`、`multiset`、`unordered_set`、`multiset`、`unordered_multiset`。
- 双元素（`{k:v, k:v}`）：`map`、`multimap`、`unordered_map`、`unordered_multimap`。
- `pair<T1,T2>`：输出 `first:second`。
- `tuple<...>`：输出 `(a, b, c)`，递归展开由 `zrt::tuple_print` 实现。

### `namespace zrt` 辅助函数
- `std::string to_str(const T&)`：使用 `std::stringstream`，`std::setprecision(8)` 转字符串。
- `void print()` / `void print(Args&&... rest)`：变参打印，参数之间补空格，末尾换行。等价于 Python 的 `print(*args)`。
- `std::ostream& print(std::ostream&, Args&&... rest)`：可指定输出流版本。

### 与异常模块的耦合

`zrt_exceptions.h` 的 `SPDLOG_ERROR_STACK_TRACE` 依赖 `zrt::to_str(boost::stacktrace::stacktrace(...))` → 依赖 `stl_dump.h`。引入异常模块时需确保同时包含它。

## 使用示例

```cpp
#include "zrtools/zrt_string.h"
#include "zrtools/str_utils.h"
#include "zrtools/stl_dump.h"

const auto parts = zrt::split("a,b,c,", ',');      // ["a","b","c",""]
const auto joined = zrt::join({"x","y"}, '|');     // "x|y"
const int port  = zrt::convert<int>("8080", 0);    // 8080

std::vector<int> v {1,2,3};
SPDLOG_INFO("items={}", zrt::to_str(v));            // items=[1, 2, 3]
```
