# 时间模块

当前项目在时间上分三层：
- `zrt_time.h` / `zrt_time-inl.h` / `zrt_time.cpp`：**新实现**，主推。`DateTime<EmTimeZone>` 模板 + 多分辨率 epoch。
- `time_buffer.h`：基于 `zrt::time_now` 的时间滑窗容器。
- `kronos.h`：**早期实现**，保留为兼容；推荐只引用其中的 `get_day_sec` / `get_update_time` 等交易日时间换算函数。

> 三个文件同时包含很容易产生符号重复（`get_epoch13` 等），实际工程里基本只 `#include "zrtools/zrt_time.h"`。
> `zrt_time.h` 顶层加了 `#ifdef __linux__` 守卫，只在 Linux 下编译。

## 常量与 epoch 取数

```
kKilo = 1000, kMega = 1e6, kGiga = 1e9
```

| API | 分辨率 | 实现 |
|-----|--------|------|
| `double zrt::get_epoch()` | 秒（double） | `system_clock::now().time_since_epoch` |
| `long zrt::get_epoch10()` | 秒 | `clock_gettime(CLOCK_REALTIME)` 派生 |
| `long zrt::get_epoch13()` | 毫秒 | `get_epoch19() / kMega` |
| `long zrt::get_epoch16()` | 微秒 | `get_epoch19() / kKilo` |
| `long zrt::get_epoch19()` | 纳秒 | `CLOCK_REALTIME` |
| `long zrt::get_epoch<T>()` | `T` | `duration_cast<T>(high_resolution_clock::now().time_since_epoch())` |
| `double zrt::time_now()` | 秒（double） | `get_epoch()` 别名 |
| `uint64_t zrt::rdtsc()` | 周期数 | 直接汇编 `rdtsc` |
| `int64_t zrt::get_monotonic19()` | 纳秒，单调 | `CLOCK_MONOTONIC` |

## 时间字符串

本地时区：
- `zrt::get_time_str_local(int64_t epoch10, format = "%FT%T")`：`strftime` + `localtime`。
- `zrt::get_time_str_local(format = "%FT%T")`：当前时间（秒精度）。

UTC：
- `zrt::GetTimeStrUTC10(int64_t epoch10, format = "%Y-%m-%dT%H:%M:%SZ")`：基于 `boost::posix_time::time_facet`。
- `zrt::GetTimeStrUTC10(format = ...)`：当前时间（秒精度）。
- `zrt::GetTimeStrUTC(format = "%Y-%m-%dT%H:%M:%SZ", decimal_places = 6)`：可带微秒；内部利用 `%f` 占位符截断到期望小数位。

解析：
- `std::tm zrt::ParseTimeStr(const std::string& time_str, const std::string& format)`：`std::get_time`，失败记 `SPDLOG_ERROR` 并返回空 `tm`。
- `int zrt::GetHMS(time_str, format)` / `GetHMS()`：返回 `H*10000 + M*100 + S`，无参版本用当前本地时间。
- `int zrt::GetYmd(time_str, format)` / `GetYmd()`：返回 `Y*10000 + M*100 + D`。

## `Timer`（RAII 计时）

```cpp
{
    zrt::Timer t("my_task");
    do_work();
}   // 析构时 SPDLOG_INFO("[my_task] time cost: 0.123456s")
```

- `double get_diff()` 读当前耗时（秒）。
- `print()` / `print(msg)` 显式打印。
- `reset()` 重置起点。

## `DateTime<EmTimeZone>`

```cpp
enum EmTimeZone { kUTC, kLocal };
template <EmTimeZone tz = kLocal> class DateTime;

using DateTimeUTC   = DateTime<kUTC>;
using DateTimeLocal = DateTime<kLocal>;
```

### 构造

| 构造器 | 语义 |
|--------|------|
| `DateTime()` | 取当前时间 (`CLOCK_REALTIME`) |
| `DateTime(std::string time_str, std::string format)` | 按 `format` 解析；支持 `.%f` 解析到纳秒 |
| `DateTime(int64_t epoch10)` | 默认视为 10 位 |
| `DateTime(int64_t epoch, int digits)` | `digits ∈ {10,13,16,19}` |

### Epoch 取数

| 方法 | 单位 |
|------|------|
| `Epoch10()` | 秒 |
| `Epoch13()` | 毫秒 |
| `Epoch16()` | 微秒 |
| `Epoch19()` | 纳秒 |
| `EpochD()` | 秒 (double) |

### 比较

`operator<`、`>`、`==`、`<=`、`>=`：按 `Epoch19()`。

### 字段访问

`GetHMS()` / `GetYmd()` / `GetYmdHMS()`（后者返回 `YYYYMMDDHHMMSS`）。

### 链式 setter / offset

- `SetYear / SetMon / SetDay / SetHour / SetMin / SetSec / SetNano`：修改 `m_tm` 后 `UpdateEpoch`；return `*this`。
- `OffsetDay / OffsetHour / OffsetMin / OffsetSec / OffsetMSec / OffsetUSec / OffsetNano`：直接加 `tv_sec` 或 `tv_nsec`；缓存 `m_cached_epoch = -1`。

### 差值

`DiffSec / DiffMill / DiffMicro / DiffNano / DiffD`（分别返回对应单位的差）。

### 格式化

```cpp
std::string ToFormat(const std::string& format, uint decimal_places);
std::string ToFormat(const std::string& format);          // decimal_places = 3
std::string ToFormat(uint decimal_places);                // format = "%FT%T.%f"
std::string ToFormat();                                    // "%FT%T.%f", 3
```

`.%f` 占位符会被替换为 `decimal_places` 位小数（来自 `tv_nsec`，定宽填充 0）。

### 性能细节

- `UTC`：用 Howard Hinnant 的 civil-from-days 算法（`detail::civil_from_epoch_utc`），绕开 `gmtime_r` 的锁，约 **7μs**。
- `Local`：走 `localtime_r`，约 **110μs**（包含 tz 文件查阅）。
- 内部 `m_cached_epoch` 缓存最近一次 `tv_sec`，相同秒内 `UpdateTM()` 命中缓存 <5ns。
- 反向 `TSUpdater<kUTC>::update` 使用 `fast_timegm`（`days_from_civil` 直算），避免 `timegm`。

## `TimeBuffer<T, Mutex = zrt::null_mutex>`（time_buffer.h）

按秒级时间窗保留元素的 FIFO：

```cpp
TimeBuffer<Tick> buf(3.0);  // 保留 3 秒
buf.Push(tick);              // push 时自动淘汰 now - front.time > limit 的数据
buf.ForEach([](const Tick& t){ ... });
```

- 元素 `Element { double time; T data; }`，时间戳来自 `zrt::time_now()`。
- `Mutex` 默认 `zrt::null_mutex`（单线程）；需要并发时传入 `std::mutex`。
- `ForEach` 会在遍历前再做一次过期淘汰，确保 callback 只看到有效数据。

> 时间源是**实时钟**，系统时间回拨会影响窗口边界；吞吐敏感场景建议换成 `CLOCK_MONOTONIC`。

## `kronos.h`（遗留）

- `get_epoch_l<T>()` / `getEpoch13()` / `getEpoch13_v2()`：老版 epoch 取数；现已被 `zrt_time.h` 取代。
- `epoch13_to_iso2(epoch13)` / `epoch10_to_iso2(epoch10)`：`strftime` 到 ISO 风格（`%Y-%m-%dT%H:%M:%SZ`）。
- `time_cost(double start)`：`std::cout` 打印耗时。
- **建议保留** 的工具：
  - `int get_day_sec(int update_time)`：把 `HHMMSS.mmm` 风格整数 `HH*10000 + MM*100 + SS` 乘 1000 的时间，转成当日秒数。
  - `int get_update_time(int day_seconds)`：反向转换。
- `#ifdef HAS_BOOST` 区块内还有 `timeStrToEpoch10/13`、`epoch10ToTimeIso`、`epoch13ToTimeIso`，默认不编译。

## 典型用法

```cpp
#include "zrtools/zrt_time.h"
#include "zrtools/zrt_time-inl.h"

// 纳秒时间戳
const int64_t nanos = zrt::get_epoch19();

// 解析 / 转换 / 格式化
zrt::DateTimeUTC dt("2024-01-01T00:00:00.123456", "%Y-%m-%dT%H:%M:%S.%f");
SPDLOG_INFO("ms={}, iso={}", dt.Epoch13(), dt.ToFormat("%FT%T.%f", 6));

// 链式偏移
const auto tomorrow_utc = zrt::DateTimeUTC{}.OffsetDay(1).ToFormat();
```
