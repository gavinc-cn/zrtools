# 数值 / 比较 / 填充模块

围绕浮点精度、字符串/数值等号语义、跨类型字段填充的工具集。

模块之间依赖：`zrt_equal.h` → `zrt_compare.h` → `zrt_math.h`、`zrt_fill.h`。

## zrt_equal.h

常量：`zrt::kFloatEpsilon = 1e-6f`、`zrt::kDoubleEpsilon = 1e-8`。

核心函数 `zrt::equal`（大量重载，统一解决跨类型判等）：

| 场景 | 行为 |
|------|------|
| 同类型 T（非数组、非浮点） | `a == b` |
| 两个 `IsIntegralNum` 类型 | `a == b` |
| `char` 与整数 | 双方字符串化再比较 |
| `char[]`、`char`、`std::string` 两两组合 | `std::string` 比较 |
| `char[]` 与整数 | `std::to_string(int) == std::string(cstr)` |
| `std::string` 与整数 | 同上 |
| 枚举与 `int`/`char` | `static_cast<int|char>(enum)` 比较 |
| `double/float` | `a == b`；双 `NaN` 视为相等；否则 `std::abs(a-b) < epsilon` |

`-Wfloat-equal` 警告在文件内已用 `#pragma` 抑制。

## zrt_compare.h

- 数值大小 `greater / less / greater_equal / less_equal`：整型走原生比较；`float/double` 用 `epsilon`；`char[]` / `char` / `std::string` 组合转 `std::string` 比较。
- `relative_close(a, b, epsilon)`：相对误差，`NaN/NaN` 视为相等。
- `is_empty(T)`：
  - 浮点：`!a`（规避 `-Wfloat-equal`）；
  - 非数组、非浮点：`!a`；
  - `char[]`：`strlen == 0`；
  - `std::string` / `zrt::StrView`：`.empty()`。
- `my_less<T>` / `my_equal<T>` / `my_hash<T>`：函数对象封装，`char*` 也能正确走 `std::string` 比较/哈希，用于 `std::map/unordered_map` 的自定义比较器。
- 变参 `zrt::max(a, b, ...)` / `zrt::min(...)`：根据 `std::common_type_t` 决定返回引用或值；对 `float/double` 的 `NaN` 视为空值并跳过（若全部 NaN，返回最后一个值）。

> 注意：`zrt::max(float, NaN)` 会返回 `float`（非 NaN），这是与 `std::max` 的主要区别。

## zrt_math.h

| API | 语义 |
|-----|------|
| `double round(num, int n)` | 四舍五入保留 `n` 位小数 |
| `double round_to(value, precision)` | 按 `precision` 步长四舍五入；`precision` 为 0 时记错并返回 `value` |
| `double floor_to(value, precision)` | 向下取整到步长 |
| `int get_decimal_places(num)` | 通过不断 `*10` 统计首个非零小数位前 0 的个数（即精度缩放所需小数位数） |
| `std::string round_str(num, n)` | 定宽 `"{:.Nf}"` 格式化 |
| `std::string round_to_str(value, precision)` / `floor_to_str(...)` | 组合调用上面两个 |

### `template<typename T> class SMA`（Simple Moving Average）

滑动均值：
- `SMA()` / `SMA(size_t window_size)`；`SetWindowSize(...)`
- `void Add(const T&)`：滑窗满时弹出最早数据
- `T GetAvg()`：当前均值（`m_sum / m_data.size()`；窗口满后走 `safe_div(..., 0)`）
- `size_t GetSize()`

> `GetAvg` 未加 const，且在空容器时返回 `T{}`。精度以 `T` 为准，内部不做浮点 epsilon 处理。

## zrt_fill.h

核心函数 `zrt::fill_field(Dst&, const Src&)`：针对业务结构体字段的跨类型赋值，**静默截断不报错，超长字符串按 `Dst` 容量截短并补 `\0`**。覆盖的组合如下：

| 目标 | 来源 | 备注 |
|------|------|------|
| `char[N]` | `char[M]` | `strnlen` 限制，`N-1` 截断补 `\0` |
| 整数 / `double` | `const char*` / `std::string` | 走 `zrt::convert` |
| `char` | `const char*` | 取首字符 |
| `std::string` | `const char*` / `zrt::StrView` / `char` | 按语义构造 |
| `char[N]` | `std::string` / `zrt::StrView` / `char` / 整数 | `strncpy` + 补 `\0`；整数先 `to_string` |
| `char` | `zrt::StrView` | 非空时取首字符 |
| `double` | 整数 | 隐式转换 |
| `int` / `char` / `char[2]` | 枚举 | `static_cast` |
| `T` | `T`（非 CStr） | 直接赋值 |
| `T` | `U`（`std::is_convertible_v<U,T>`） | 直接赋值 |

辅助：
- `on_err_log(dst, src)` / `on_err_fill_default(dst, src)`：错误回调的模板（仅记日志 / 记日志并重置为 `Dst{}`）。
- `equal_or_fill(dst, src)`：不等时记日志并填充。
- `equal_or_pass(dst, src)`：不等时只记日志。
- `fill_if_not_empty(dst, src)`：`!is_empty(src)` 才填充。
- 宏 `ZRT_EQUAL_OR_PASS(dst, src)`：和 `equal_or_pass` 语义一致，但把变量名输出到日志。

## 示例

```cpp
#include "zrtools/zrt_math.h"
#include "zrtools/zrt_fill.h"

// 报价四舍五入到 tick
double price = zrt::round_to(101.23456, 0.01);          // 101.23
std::string text = zrt::floor_to_str(101.23456, 0.001);  // "101.234"

struct Tick { char symbol[16]; double px; };
Tick t {};
zrt::fill_field(t.symbol, std::string("BTCUSDT"));
zrt::fill_field(t.px, "12345.6");     // 走 convert<double>

// 滑窗均值
zrt::SMA<double> sma(5);
for (double p : prices) sma.Add(p);
const double avg = sma.GetAvg();
```
