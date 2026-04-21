# Bollinger 滚动窗口

`bollinger.h` 只有一个类 `Bollinger`，提供**定长滑动窗口**的均值、标准差、布林带上下轨。`namespace` 是全局（**不在** `zrt` 下）。

```cpp
class Bollinger {
public:
    explicit Bollinger(size_t size = 1);

    bool   SetWindowSize(size_t size);      // 只允许变大；变小返回 false 不生效
    void   SetStdMulti(int multi);          // 默认 2

    void   AddValue(double value);          // O(1)，超限自动淘汰 front
    double GetAverage() const;              // 空窗口返回 0
    double GetStdDev() const;               // 总体标准差（除以 N，非 N-1）
    double GetUpperBound() const;           // mean + k * std
    double GetLowerBound() const;           // mean - k * std
    void   GetBollinger(double& upper, double& middle, double& lower) const;  // 一次算三个

private:
    size_t             m_window_size;
    std::deque<double> m_window;
    double             sum {};
    double             sum_squares {};
    int                m_std_multiple = 2;
};
```

## 语义要点

- **滚动窗口**：`deque<double>`；push 时若已满则先 `pop_front` 并从 `sum / sum_squares` 里减掉 front 及其平方。整个 `AddValue` O(1)。
- **方差公式**：`variance = E[x^2] - E[x]^2`。浮点误差会随累积增长；窗口很长或数值跨度很大时建议用 Welford 重写（当前实现不做）。
- **`SetWindowSize` 只进不退**：传入比当前小的尺寸直接返回 `false`，窗口内容不动。想缩窗口需要重新构造。
- **`SetStdMulti(int)` 参数是 int**：传小数（如 1.5）会被截断。需要小数倍数请自行把上轨算 `GetAverage() + 1.5 * GetStdDev()`。
- **空窗口**：`GetAverage/GetStdDev/GetUpperBound/GetLowerBound` 都返回 0；`GetBollinger` 直接 `return`，不修改输出参数。
- **非线程安全**：任何并发读写都要外部加锁。

## 典型用法

```cpp
#include "zrtools/bollinger.h"

Bollinger bb(20);           // 20 根窗口
bb.SetStdMulti(2);          // 2σ 布林带

for (double px : prices) {
    bb.AddValue(px);
}

double up, mid, lo;
bb.GetBollinger(up, mid, lo);
SPDLOG_INFO("BB up={:.2f} mid={:.2f} lo={:.2f}", up, mid, lo);
```

## 注意

- 头文件**没有 include** `<deque>` / `<cmath>`（依赖调用方先 include 或传递 include），独立使用时加上：
  ```cpp
  #include <deque>
  #include <cmath>
  #include "zrtools/bollinger.h"
  ```
- 总体标准差（`/N`）而非样本标准差（`/(N-1)`）。如果要和 pandas `rolling().std()` 对齐，需要自行乘以 `sqrt(N / (N-1))`。
- 类**不在 `zrt` 命名空间**；引入时直接 `Bollinger bb;`。
