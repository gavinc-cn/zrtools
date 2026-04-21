# io_pool_v2

`io_pool_v2` 是一个基于"线程抽象 + 消息派发"的任务处理框架。它提供了可插拔的工作线程实现（Boost.Asio、无锁队列、互斥队列、MoodyCamel 并发队列、同步直执等），并在其上构建了消息处理器（Handler）、服务池（Pool）等上层组件。

---

## 1. 总览

```
┌─────────────────────────────────────────────────────────────┐
│                         上层组件                              │
│                                                              │
│   IHandler<Buffer,Cb,SyncCb>      IOService<...>            │
│   (组合 IThread, 与 EnginePool     (继承 Engine, 内建派发)   │
│    集成计数)                                                 │
│                   │                          │               │
│                   ▼                          ▼               │
│   EnginePool (singleton)            IOPool<Service>          │
│   - 按名字/匿名管理 IThread*         - 按名字/匿名管理 Service│
│   - 任务计数 (post/handle/pending)                          │
└──────────────────────────────────────┬──────────────────────┘
                                       │
                                       ▼
┌─────────────────────────────────────────────────────────────┐
│                    IThread (抽象接口)                         │
│                                                              │
│  virtual bool ThreadStart() / ThreadStop() / Post(task)     │
│  virtual bool IsInThread() const                            │
│  virtual boost::asio::io_service* RefIoService()            │
└─────────┬─────────┬─────────┬───────────┬─────────┬─────────┘
          │         │         │           │         │
      SyncThread BoostAsio  LockDeque  BoostLockfree MoodyCamel
                   Thread    Thread     Thread        Thread
```

模块定位：

- **底层**：`IThread` 接口 + 多种实现，统一"向某个执行上下文投递任务"的能力。
- **中间层**：`LockDeque`（线程安全 deque，供 `LockDequeThread` 使用）。
- **上层**：`EnginePool` / `IOPool` 管理一组线程；`IHandler` / `IOService` 在线程之上挂载消息派发逻辑。

---

## 2. 文件清单

| 文件 | 内容 |
| --- | --- |
| `i_thread.h`               | `IThread` 抽象基类 |
| `i_handler.h`              | `IHandler<Buffer, Callback, SyncCallback>` 消息处理器基类 |
| `sync_thread.h`            | `SyncThread` — 直接在调用线程同步执行 |
| `boost_asio_thread.h`      | `BoostAsioThread` — 基于 `boost::asio::io_service` 的工作线程 |
| `lock_deque.h`             | `LockDeque<T>` — mutex 保护的 deque |
| `lock_deque_thread.h`      | `LockDequeThread` — 基于 `LockDeque` 的工作线程 |
| `boost_lockfree_thread.h`  | `BoostLockfreeThread` — 基于 `boost::lockfree::queue` 的工作线程 |
| `moodycamel_thread.h`      | `MoodyCamelThread` — 基于 `moodycamel::ConcurrentQueue` 的工作线程 |
| `engine_pool.h`            | `zrt::EnginePool` 单例，线程池管理 + 任务计数 |
| `io_service.h`             | `IOService<...>` 模板：继承 Engine + 内建消息派发 |
| `io_pool.h`                | `IOPool<Service>` 模板：`shared_ptr<Service>` 的池 |

---

## 3. `IThread` — 线程抽象

```cpp
class IThread : public boost::noncopyable {
public:
    virtual bool ThreadStart() = 0;
    virtual bool ThreadStop()  = 0;
    virtual bool Post(const std::function<void()>& task) = 0;

    virtual boost::asio::io_service* RefIoService();  // 默认返回 nullptr
    virtual bool IsInThread() const;                  // 默认对比线程 ID

protected:
    void SetCurrentThreadId();  // 工作线程启动后自行调用
};
```

### 语义约定

- **`ThreadStart` / `ThreadStop` 幂等**：重复调用应返回 `true` 且无副作用。
- **`Post(task)` 线程安全**：任意线程可投递；任务在工作线程中 FIFO 执行（`BoostLockfreeThread` / `MoodyCamelThread` 对并发生产者友好）。
- **`IsInThread()`**：用于同步调用防死锁检测（见 `IHandler::PostSyncMsg`）。工作线程在自己的 run-loop 中必须先调用 `SetCurrentThreadId()` 后才能让 `IsInThread` 返回正确值。
- **`RefIoService()`**：只有持有 `boost::asio::io_service` 的实现才应重写（目前仅 `BoostAsioThread`）。

继承 `boost::noncopyable`：禁止复制，但不禁止移动（`BoostAsioThread` 显式提供移动构造）。

---

## 4. 线程实现对比

| 实现 | 队列 | 是否真·多线程 | 依赖 | 典型用途 |
| --- | --- | --- | --- | --- |
| `SyncThread`         | 无，直接执行     | 否        | -                                | 单元测试、调试 |
| `BoostAsioThread`    | `io_service`    | 是        | Boost.Asio                       | 与 Asio 网络 I/O 混用 |
| `LockDequeThread`    | `std::deque`+mutex | 是     | std                              | 简单通用、可预期延迟 |
| `BoostLockfreeThread`| `boost::lockfree::queue` cap=65534 | 是 | Boost.Lockfree | 高并发生产者 |
| `MoodyCamelThread`   | `moodycamel::ConcurrentQueue` | 是 | moodycamel | 高并发生产者/消费者 |

### `SyncThread`

- `Post(task)` 立即在调用线程执行。
- `IsInThread()` 恒为 `true`（任何线程调用都视作"本线程"）——符合同步语义。
- 适合单元测试，不引入时序不确定性。

### `BoostAsioThread`

- 启动一个 `std::thread` 跑 `io_service::run()`，利用 `io_service::work` 保持事件循环存活。
- `Post` 转发到 `io_service::post`。
- 停止时先 `io_service::stop()` 再 `join()`。
- 提供 `RefIoService()` 以便上层复用 Asio 的 timer、socket 等。

### `LockDequeThread`

- 工作线程在 `while(m_is_started)` 循环中 `pop_front` 或 `yield`。
- 注意：`Post` 在入队后没有 notify，消费端靠 `yield` 忙轮询 — **CPU 占用偏高**，停机延迟上限是一个 yield 周期。

### `BoostLockfreeThread`

- 队列存 `Task*`，`Post` 中 `new Task(task)` 入队，消费端 `delete`。
- 队列容量编译期固定为 65534。
- **注意**：`Post` 未检查 `push` 是否成功——队列满时会直接返回 `false`，但已分配的 `task_ptr` 不会被释放（内存泄漏）。

### `MoodyCamelThread`

- 需外部提供 moodycamel 单头文件。
- 队列按值存储 `Task`（`std::function<void()>`），无需额外堆分配。

---

## 5. `LockDeque<T>`

极简的互斥包装：

```cpp
template<typename T>
class LockDeque {
    bool push_back(const T& t);  // 总是返回 true
    bool pop_front(T& t);        // 空时返回 false
private:
    std::deque<T> m_q;
    std::mutex m_mutex;
};
```

只为 `LockDequeThread` 服务。与 `boost::lockfree::queue` / moodycamel 的区别是无容量上限 + 阻塞互斥。

---

## 6. `EnginePool`（单例）

```cpp
namespace zrt {
class EnginePool {
    ZRT_DECLARE_SINGLETON(EnginePool);
public:
    template<typename T> void AddNamedEngine(const std::string& name);
    template<typename T> void AddSharedEngine(int n);

    void EngineStart();
    void EngineStop();
    bool IsStarted() const;

    IThread* GetNamedThread(const std::string& name);
    IThread* GetSharedThread();   // 轮询返回匿名线程

    void AddPostCnt();            // 由 IHandler::PostMsg 调用
    void AddHandleCnt();          // 由 IHandler::OnMessage 调用
    bool AllTaskDone() const;
    void WaitStop();              // 轮询等待 unhandled 归零后 stop
};
} // namespace zrt
```

### 生命周期

1. 注册：`AddNamedEngine<BoostAsioThread>("net_io")` / `AddSharedEngine<LockDequeThread>(4)`。
2. 启动：`EngineStart()` — 逐个 `ThreadStart`。
3. 运行：通过 `GetNamedThread` / `GetSharedThread` 拿到 `IThread*` 供 `IHandler` 绑定。
4. 停机：`EngineStop()` 或 `WaitStop()`（后者等所有已投递任务处理完毕）。

### 所有权

`EnginePool` **持有原生 `IThread*`**（`new T()` 构造），析构时 `delete`。不要将同一指针同时交给其他容器。

### 计数

- `m_post_cnt` / `m_handle_cnt` / `m_unhandled_cnt` 三个原子计数。
- 由 `IHandler` 显式调用 `AddPostCnt` / `AddHandleCnt`。**如果不通过 `IHandler` 投递（直接 `IThread::Post`），计数会失准。**
- `WaitStop` 循环 `sleep(1)` 直到 `m_unhandled_cnt == 0`——POSIX `sleep`，**Windows 下不可移植**。

---

## 7. `IHandler<Buffer, Callback, SyncCallback>`

消息处理器基类：把"收到一个 `msg_type + Buffer`"分发给已注册的回调。

### 模板参数

| 参数            | 示例                                                           |
| --------------- | -------------------------------------------------------------- |
| `Buffer`        | `std::string`、`std::vector<char>`、自定义 POD 等             |
| `Callback`      | `std::function<void(int msg_type, const Buffer&)>`             |
| `SyncCallback`  | `std::function<void(int, const Buffer&, std::promise<Buffer>&)>` |

### 核心方法

```cpp
void InstallHandler(int msg_type, const Callback& cb);
void InstallSyncHandler(int msg_type, const SyncCallback& cb);
void InstallDefaultHandler(const Callback& cb);
void InstallDefaultSyncHandler(const SyncCallback& cb);

void PostMsg(int msg_type, const Buffer& buffer);            // 异步
void PostSyncMsg(int msg_type, const Buffer buffer, Buffer& result);  // 同步

void SetThread(IThread* thread);
IThread* GetThread() const;
boost::asio::io_service* RefIoService() const;  // 透传到 IThread
```

### 派发逻辑

1. **查专用表** `m_msg_handler_map` → 调用。
2. **否则** 用 `m_default_handler`。
3. **否则** 记错并丢弃。

无论哪条路径，最终都会 `AddHandleCnt()`——用于 `EnginePool::WaitStop` 的任务平衡。

### 同步调用防死锁

`PostSyncMsg` 会检测 `GetThread()->IsInThread()`：

- **若在本线程调用**：直接同步执行，避免"自己投递给自己、自己等待自己"的死锁。
- **否则**：`Post` 到工作线程，阻塞在 `future.get()`。

这一机制依赖 `IThread::IsInThread()` 的正确实现。

### 注册冲突

`InstallHandler` / `InstallSyncHandler` 都会更新 `m_msg_id_set`。若同一 `msg_type` 被注册两次（不论同类还是跨 handler/sync），会打 `SPDLOG_ERROR` 但**仍然覆盖**旧回调。两张 map 共用同一 id 集，因此 `InstallHandler(1, ...)` 之后再 `InstallSyncHandler(1, ...)` 也会触发告警。

---

## 8. `IOService<Buffer, Callback, SyncCallback, Engine>`

老一代"线程 + 派发"融合实现：

```cpp
template<typename Buffer, typename Cb, typename SyncCb, typename Engine>
class IOService : public Engine { ... };
```

- **继承** 一个具体 Engine（如 `BoostAsioThread`）。
- 派发逻辑与 `IHandler` 类似，但**不**与 `EnginePool` 计数集成。
- 提供 fluent 安装接口：

  ```cpp
  service.InstallHandler()
    (1, cb1)
    (2, cb2);
  ```

与 `IHandler` 的差别：
- `IOService` 通过继承组合线程；`IHandler` 通过 `IThread*` 组合。
- `IOService` 的 `Init()` 是纯虚——子类必须实现。
- `IOService` 没有"默认处理器不存在则记 handle 计数"这一步。

新代码建议使用 `IHandler + EnginePool`，`IOService` 保留用于旧接口兼容。

---

## 9. `IOPool<Service>`

```cpp
struct IOPoolConfig {
    std::vector<std::string> named_threads;
    int anonymous_thread_num;
};

template<typename Service>
class IOPool {
    void Init();
    void Start();
    void Stop();
    bool IsStarted() const;

    std::shared_ptr<Service> GetNamedThread(const std::string& name);
    std::shared_ptr<Service> GetSharedThread();  // 轮询
    bool Register(const std::string& name, std::shared_ptr<Service> s);
};
```

- 泛型参数 `Service` 一般是 `IOService` 派生类。
- 与 `EnginePool` 的对比：
  - `IOPool` 用 `shared_ptr` + 外部注入；`EnginePool` 用裸指针 + 内部构造。
  - `IOPool` 不做任务计数。
  - `IOPool` 是**实例化类**（每个业务一份）；`EnginePool` 是**单例**。

### 已知 Bug

- `IOPool::Init()` 对"已启动"的判断用了 `m_is_started`，但该字段在 `Start()` 才设置 —— 正常情况下 `Init` 前 `m_is_started` 必为 false，这段早返回分支形同死码。
- `IOPool::Register` 的 `SPDLOG_ERROR("service {} already exists")` 缺少参数绑定，输出会保留 `{}` 字样。
- `IOPool::Init` 里对匿名线程打的是 `"init named anonymous service"`（"named" 多余）。

这些是迁移前就存在的行为，**不建议**在这次迁移中一起修；等单测覆盖之后再重构更安全。

---

## 10. 典型用法

### A. 用 `EnginePool + IHandler`

```cpp
using Buffer = std::string;
using Callback     = std::function<void(int, const Buffer&)>;
using SyncCallback = std::function<void(int, const Buffer&, std::promise<Buffer>&)>;

class MyHandler : public IHandler<Buffer, Callback, SyncCallback> {
public:
    bool Init() override {
        InstallHandler(/*msg_type=*/1, [](int, const Buffer& b) {
            SPDLOG_INFO("got: {}", b);
        });
        return true;
    }
    bool Start() override { return true; }
};

int main() {
    auto& pool = zrt::EnginePool::GetInstance();
    pool.AddNamedEngine<BoostAsioThread>("net");
    pool.AddSharedEngine<LockDequeThread>(4);
    pool.EngineStart();

    MyHandler h;
    h.SetThread(pool.GetNamedThread("net"));
    h.Init();
    h.Start();

    h.PostMsg(1, "hello");

    pool.WaitStop();
}
```

### B. 用 `IOService` 继承风格

```cpp
class MyService : public IOService<Buffer, Callback, SyncCallback, BoostAsioThread> {
public:
    bool Init() override {
        InstallHandler()(1, [](int, const Buffer& b){ /*...*/ });
        return true;
    }
};

MyService svc;
svc.Init();
svc.ThreadStart();
svc.PostMsg(1, "hello");
```

---

## 11. 已知陷阱清单

| # | 位置 | 问题 | 风险 |
| - | --- | --- | --- |
| 1 | `io_pool.h:106` | `SPDLOG_ERROR("service {} already exists")` 未传 `service_name` | 日志不可读 |
| 2 | `io_pool.h:41` | `Init()` 里匿名线程分支的日志写成 "init named anonymous service" | 日志误导 |
| 3 | `io_pool.h:23`  | `Init()` 检查 `m_is_started` 但该字段直到 `Start()` 才被置位 | 死代码 |
| 4 | `boost_lockfree_thread.h:37` | 队列满时 `Post` 失败但 `new Task(task)` 的分配未释放 | 内存泄漏 |
| 5 | `engine_pool.h:97` | `WaitStop` 用 POSIX `sleep(1)` | 不可移植 |
| 6 | `i_handler.h:30/37` | 同一 `msg_type` 重复注册只告警不拒绝 | 覆盖旧回调 |
| 7 | `lock_deque_thread.h` / `boost_lockfree_thread.h` | 消费端 `yield` 忙轮询 | 空闲 CPU 占用 |

---

## 12. 单元测试策略

对应的 gtest 套件位于 `tests/io_pool_v2/`。覆盖策略：

| 组件 | 测试重点 |
| --- | --- |
| `LockDeque`           | push/pop 顺序、空队列 pop 返回 false、多线程并发正确性 |
| `SyncThread`          | `Post` 同步执行、`IsInThread` 恒真、幂等 start/stop |
| `BoostAsioThread`     | `Post` 异步执行、`RefIoService` 非空、`IsInThread` 仅在工作线程内为真 |
| `LockDequeThread`     | `Post` 投递后任务最终执行、停机后不再执行新任务 |
| `BoostLockfreeThread` | 同上 + 压满队列的边界（注意已知泄漏） |
| `MoodyCamelThread`    | 同上 |
| `EnginePool`          | 命名/匿名线程注册、轮询、计数、`WaitStop` 平衡 |
| `IHandler`            | Handler 派发、默认 handler 兜底、同步消息防死锁、计数与 EnginePool 匹配 |
| `IOService`           | 安装 fluent 接口、派发、同步消息 |
| `IOPool`              | 注册冲突、轮询、生命周期 |

测试框架使用 GoogleTest，通过 CMake `FetchContent` 拉取，不引入系统依赖。
