# 并发模块

只有三个小文件，均在 `namespace zrt`：线程亲和性 / 调度辅助、空互斥量、读写锁 Hash Map。

## thread.h

线程属性设置（Linux/pthread 接口）。

```cpp
namespace zrt {
    bool SetThreadAffinity(pthread_t thread, int cpu_id);
    bool SetThreadSched(pthread_t thread, int policy, const sched_param& param);
}
```

- `SetThreadAffinity`：`CPU_ZERO + CPU_SET + pthread_setaffinity_np`。失败记日志返回 `false`。
- `SetThreadSched`：`pthread_setschedparam`。常配合 `SCHED_FIFO`/`SCHED_RR` 及实时 priority 使用，**需 `CAP_SYS_NICE` 或 root**。

示例：

```cpp
std::thread worker([]{ do_work(); });
zrt::SetThreadAffinity(worker.native_handle(), 2);
sched_param sp { .sched_priority = 50 };
zrt::SetThreadSched(worker.native_handle(), SCHED_FIFO, sp);
```

## mutex.h

```cpp
namespace zrt {
    struct null_mutex {
        void lock() {}
        void unlock() {}
        bool try_lock() { return true; }
    };
}
```

用于**静态分派**：把模板类的 `Mutex` 参数特化成它，就能在单线程场景下完全抹掉锁开销，同时保留 `std::lock_guard<Mutex>` 的语法。

> `ultraq.h` 内也定义了同名 `null_mutex`，但不在 `zrt` 下；避免 ADL 歧义，**多引入 `ultraq.h` 时建议显式加 `zrt::` 前缀**。

使用场景：
- `time_buffer.h::TimeBuffer<T, Mutex = zrt::null_mutex>`
- `ultraq.h::UltraQ_ST`（即 `UltraQ<null_mutex>`）

## zrt_locked_hash_map.h

`LockedHashMap<Key, Value, Hash = std::hash<Key>, KeyEqual = std::equal_to<Key>, Allocator = ...>`：基于 `std::shared_mutex` 的线程安全 `unordered_map`。

### 设计

- **禁止拷贝、禁止移动**（`delete` 拷贝/移动构造与赋值）。
- 读操作用 `std::shared_lock<std::shared_mutex>`；写操作用 `std::lock_guard<std::shared_mutex>`。多读并发，写互斥。
- 返回**值拷贝**而非引用，避免调用方解锁后访问失效。

### 类型别名

```cpp
using key_type      = Key;
using mapped_type   = Value;
using value_type    = std::pair<const Key, Value>;
using size_type     = std::size_t;
using hash_map_type = std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>;
```

### 写 API

| 方法 | 返回 | 语义 |
|------|------|------|
| `bool insert_or_assign(key, value)` | 是否新插入 | 走 `std::unordered_map::insert_or_assign` |
| `bool insert(key, value)` | 是否插入成功 | key 已存在时失败返回 `false` |
| `bool emplace(key, Args&&...)` | 是否插入成功 | 用 `Args` 构造 `Value` |
| `Value operator[](key)` | 当前值（拷贝） | 不存在时插入默认构造值 |
| `bool erase(key)` | 是否删除 | |
| `void clear()` | — | |
| `bool update_if_exists(key, func)` | 是否找到并更新 | `func(const Value&) -> Value` |
| `void for_each_mutable(func)` | — | `func(const Key&, Value&)`，持写锁 |

### 读 API

| 方法 | 返回 | 语义 |
|------|------|------|
| `std::optional<Value> find(key) const` | `nullopt` 或值 | |
| `Value get_or_default(key, default_value) const` | 值 | |
| `bool contains(key) const` | 是否包含 | |
| `size_type size() const` / `bool empty() const` | — | |
| `void for_each(func) const` | — | `func(const Key&, const Value&)`，持读锁 |
| `std::vector<Key> keys() const` / `std::vector<Value> values() const` | 快照 | |

### 并发注意

- `for_each` / `for_each_mutable` 会**长时间持锁**，回调里**不要再操作本 map**（死锁/升级锁失败）。需要更新可以配合 `keys()` 取快照后再逐个 `update_if_exists`。
- `operator[]` 返回的是拷贝，不能像标准 map 那样做 `m[k].field = x`；请用 `update_if_exists` 或先 `find` 再 `insert_or_assign`。
- `std::shared_mutex` 在 glibc 下写优先，大量读时写会被拖长；如果读极多写极少，可考虑 `std::shared_mutex + std::atomic`，或 RCU 风格方案。

### 示例

```cpp
#include "zrtools/zrt_locked_hash_map.h"

zrt::LockedHashMap<std::string, int> counters;

counters.insert_or_assign("hit", 0);
counters.update_if_exists("hit", [](const int& v){ return v + 1; });

const auto snapshot = counters.keys();
for (const auto& k : snapshot) {
    SPDLOG_INFO("{}: {}", k, counters.get_or_default(k, 0));
}
```

## 相关模块

- 线程生命周期 / 任务调度：见 [io_pool_v2.md](io_pool_v2.md)。
- 底层锁队列 / 无锁队列选型：见 `io_pool_v2` 中 `lock_deque_thread / boost_lockfree_thread / moodycamel_thread`。
- 共享内存协作：见 [shm.md](shm.md)。
