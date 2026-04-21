# 缓冲 & 队列模块

两种链表块结构：
- `UltraBuf`：字节流缓冲区（无消息边界）。
- `UltraQ<Mutex>`：带 `Header` 的消息队列（消息边界由 `body_size` 记录）。

两者块大小都是编译期宏，默认 1MB。空块回收走 `m_empty_head` 链表，避免重复 `new/delete`。

## ultrabuf.h / .cpp

```cpp
namespace zrt {
    class UltraBuf: public boost::noncopyable {
    public:
        UltraBuf(size_t max_block_no = 0);
        int64_t push(const void* data, size_t size);
        int64_t pull(void* data, size_t size);        // 读走并清空
        int64_t read(void* data, size_t size) const;   // 只读快照
        bool    is_full(size_t incoming_sz) const;
        bool    is_empty(size_t size = 1) const;
        size_t  get_size() const;                      // 已用字节
        size_t  get_capacity() const;                  // 总块数 * BLOCK_BUFFER_SIZE
        size_t  cnt_block_used() const;
        size_t  cnt_block_total() const;               // 含空闲块
        void    print() const;
        void    shrink_to_fit();                       // 回收空闲链表
        std::string copy_content() const;
    };
}
```

- 宏 `BLOCK_BUFFER_SIZE` 默认 `1024*1024`（1MB），可在 include 前 `#define` 覆盖。
- `max_block_no == 0` 表示不限块数。非 0 时 `push` 在块满且已用块数等于上限时返回 `EAGAIN`。
- `push` 会跨块写入；`pull` 读满后释放头部块进入 `m_empty_head` 回收链。
- `read` 和 `pull` 的差异：`read` 不改变游标，也不回收块，只拷贝内容。
- `copy_content` 返回整段数据的 `std::string`，适合做快照。
- **非线程安全**（不加锁）。需要并发请外部套锁。
- 不允许拷贝（`boost::noncopyable`）。

### 典型用法

```cpp
zrt::UltraBuf buf;                       // 无上限
buf.push(data, len);
char tmp[1024];
const int64_t got = buf.pull(tmp, sizeof(tmp));  // 取走 min(size, buf.size)
```

## ultraq.h

模板消息队列：

```cpp
template<typename Mutex>
class UltraQ {
public:
    explicit UltraQ(size_t max_block_no = 0);
    int64_t push(const void* data, size_t size);     // 返回 size / 0（满）/ -1（单条过大）
    int64_t pull(void* data);                         // 返回 body 字节数；data 缓冲需足够大
    bool    is_full(size_t incoming_sz);
    bool    is_empty();
    size_t  get_size();                               // 所有消息 body 字节合计
    size_t  get_capacity();
    size_t  cnt_block_used();
    size_t  cnt_block_total();
};

using UltraQ_MT = UltraQ<std::mutex>;
using UltraQ_ST = UltraQ<null_mutex>;
```

- 宏 `ULTRAQ_BLOCK_BUFFER_SIZE` 默认 `1024*1024`。
- **单条消息最大** `ULTRAQ_BLOCK_BUFFER_SIZE - sizeof(Header)`；超过则 `push` 返回 `-1`。
- 每条消息格式：`[Header{uint64_t body_size}][body]`；`Header::body[0]` 是柔性数组。
- `pull(void* data)` 无 size 参数：调用方必须保证 `data` 至少能容纳最长消息，否则发生缓冲区溢出——队列没有协议层防护。
- `is_full(size_t incoming_sz)`：`max_block_no` 设置且所有块都用完时才满；否则永远返回 `false`。
- `Mutex` 推荐两种：`std::mutex`（多线程）或 `zrt::null_mutex`（单线程，零开销）。

> 注意：`ultraq.h` 内声明的 `null_mutex` 在 `namespace zrt` 下，与 `mutex.h::zrt::null_mutex` 同名同范围。若同时 `#include "zrtools/mutex.h"` 会触发重复定义。实践中二者选其一，或在 include 前用 `#define` 封闭。

### 与 UltraBuf 的区别

| | `UltraBuf` | `UltraQ` |
|--|------------|----------|
| 消息边界 | 无（字节流） | 有（每条 `Header + body`） |
| 线程安全 | 外部负责 | 模板注入 `Mutex` |
| `pull` 语义 | 取 `size` 字节 | 取一条消息 |
| 典型场景 | bitstream/串联缓冲 | 生产者-消费者消息队列 |

### 典型用法

```cpp
#include "zrtools/ultraq.h"

struct Msg { int id; char payload[32]; };
zrt::UltraQ_MT q(/*max_block_no=*/8);

Msg m { 42, "hello" };
q.push(&m, sizeof(m));

Msg out {};
if (q.pull(&out) > 0) {
    SPDLOG_INFO("got id={}", out.id);
}
```

### 已知限制 / 注意

1. `push` 跨块时分配新块走 `new Block()`，块内部 `char buf[BLOCK_BUFFER_SIZE]` 直接在堆上分配 ~1MB；高频创建/销毁 `UltraBuf/UltraQ` 会显著抖动。
2. `pull` 不清零 `begin..end` 之外区域，读时靠 `left_idx / right_idx` 控制。若做 `dump` 调试需自行避开脏数据。
3. `shrink_to_fit` 只在 `UltraBuf` 提供，`UltraQ` 当前不支持主动回收空闲块。
4. `UltraQ::is_empty()` 走锁，高频调用会有成本；可在消费线程一次性 `pull` 到空。
