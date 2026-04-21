# 共享内存模块

四种共享内存工具，按复杂度递增：

| 头文件 | 用途 | 特点 |
|--------|------|------|
| `zrt_shm.h` | 命名对象 IPC | `managed_shared_memory`，按 key 存/取 |
| `zrt_shm_direct.h` | 单线程裸映射 | 零开销，`operator->` 直接访问 |
| `zrt_shm_checkpoint.h` | 双 buffer SeqLock | 单写多读，写端崩溃不坏读端 |
| `zrt_shm_ring.h` | WAL 环形缓冲 | 变长条目、SPSC、崩溃恢复 |

所有头文件都依赖 `boost::interprocess` 和 Linux `mlock`。

## zrt_shm.h — 基础 Key/Value

两个全局模板函数（不带命名空间）：

```cpp
template<typename T>
bool WriteShm(const std::string& shm_name, const std::string& data_key, const T& src_data);

template<typename T>
bool ReadShm(const std::string& shm_name, const std::string& data_key, T& dst_data);
```

- 共享内存段固定 64KB（`managed_shared_memory`）。
- `WriteShm` **会先 `remove` 再 `create_only`** —— 每次调用都重建整段内存，不是增量写。
- `ReadShm` 只做 `open_only + find<T>(key)`，失败走 `SPDLOG_ERROR` 返回 `false`。
- `T` 必须满足 Boost.Interprocess 对共享内存对象的要求（trivially copyable 或使用 shared allocator）。

典型场景：偶发跨进程配置下发、单次状态快照。对高频读写请改用 `DirectShm` / `ShmCheckpoint`。

## zrt_shm_direct.h — 零开销单线程访问

```cpp
template<typename T>
class zrt::DirectShm {
    explicit DirectShm(const std::string& shm_name);

    T*       operator->();
    T&       operator*();
    T*       Get();
    T        Read() const;           // 返回拷贝
    void     Write(const T& src);
    void     Reset();                // *data_ = T{}
    bool     IsValid() const;
};

template<typename T>
DirectShm<T>& GetDirectShm(const std::string& shm_name);  // thread_local 缓存
```

- `T` 必须 `std::is_trivially_copyable`。
- 头部结构 `DirectShmHeader { uint32_t magic; uint32_t data_size; }`，`MAGIC_VALUE = 0x5A525431`（"ZRT1"）。
- 生命周期：`open_only` 失败→`remove`→`create_only`→`truncate(sizeof(Header)+sizeof(T))`。
- 构造时执行 `mlock`，避免页被 swap（失败仅 `SPDLOG_DEBUG`）。
- 可移动，禁止拷贝。
- **没有任何同步**：仅适合一个线程读写；多进程并发需要上层自行保证顺序。
- `GetDirectShm` 内部用 `thread_local std::unordered_map` 缓存，同一线程里按名字返回同一实例。

```cpp
struct Snapshot { double bid; double ask; int64_t ts; };

auto& shm = zrt::GetDirectShm<Snapshot>("/strategy/snap");
shm->bid = 65000.0;
shm->ask = 65001.0;
shm->ts  = zrt::get_epoch19();

Snapshot copy = shm.Read();
```

## zrt_shm_checkpoint.h — SeqLock 双 buffer

为「策略主线程周期性写，备机/监控读」的场景设计：写端崩溃在半中间状态不会让读端读到脏数据。

```cpp
template<typename State>
class zrt::ShmCheckpoint {
    explicit ShmCheckpoint(const std::string& shm_name);

    void     Write(const State& state);
    bool     Read(State& out) const;
    uint64_t GetVersion() const;           // 当前 active slot 的 seq
    bool     IsValid() const;
    void     Reset();
};

template<typename State>
class zrt::ShmCheckpointReader;            // 只读版，open_only + read_only
```

### 内存布局

```
+---------------------+ 64B
| ShmHeader           |   magic=0x43484B50 "CHKP", version, atomic<int> active
+---------------------+
| ShmSlot[0]          |   alignas(64) { atomic<uint64_t> seq; State data; }
+---------------------+
| ShmSlot[1]          |   同上
+---------------------+
```

`static_assert(sizeof(ShmHeader) == 64)`，slot 也是 64 字节对齐避免 false sharing。

### SeqLock 协议

写入（`Write`）：
1. 读 `active`，写非活跃 slot；
2. `slot.seq = old+1`（奇数，写入中）；
3. `memcpy(&slot.data, &state, sizeof(State))`；
4. `slot.seq = old+2`（偶数，完成）；
5. `active.store(write_slot)`。

读取（`Read` / `ShmCheckpointReader::Read`）：
1. 读 `active`，试读活跃 slot；
2. `seq1 = slot.seq`；若 `seq1 & 1` 正在写，重试（最多 3 次）；`seq1 == 0` 表示从未写过，返回 `false`；
3. `memcpy` 到输出；
4. `seq2 = slot.seq`；`seq1 == seq2` 即数据一致；否则重试；
5. 重试耗尽后尝试另一个 slot。

### 约束

- `State` 必须 `trivially_copyable` 且**非 polymorphic**（`static_assert`）。
- 单写者；多写者需要外部串行化。
- `mlock` 失败仅记 debug。
- `Reset` 会把两个 slot 都清零。

```cpp
struct StratState { double pnl; int pos; int64_t last_update_ns; };

zrt::ShmCheckpoint<StratState> ckpt("/strategy/ckpt");
ckpt.Write({1234.5, 10, zrt::get_epoch19()});

zrt::ShmCheckpointReader<StratState> rd("/strategy/ckpt");
StratState s{};
if (rd.Read(s)) SPDLOG_INFO("pnl={}", s.pnl);
```

## zrt_shm_ring.h — 变长 WAL 环形缓冲

```cpp
template<typename TypeEnum,
         size_t BufferSize   = 16 * 1024 * 1024,
         size_t MaxEntrySize = 4096>
class zrt::ShmRing;
```

单生产者单消费者、变长条目、可做崩溃恢复和主备同步。条目格式复用 [wal.md](wal.md) 中的 `WalEntryHeader<TypeEnum>` + `data[]`。

### 内存布局

```
+------------------+ HEADER_SIZE = 512B
| ShmRingHeader    |  magic=0x5A525752 "ZRWR", version, buffer_size
|                  |  alignas(64) atomic<uint64_t> write_pos
|                  |  alignas(64) atomic<uint64_t> read_pos
|                  |  alignas(64) atomic<uint64_t> confirmed_pos
|                  |  alignas(64) atomic<uint64_t> next_seq    (从 1 开始)
|                  |  total_entries / total_bytes / wrap_count
+------------------+
| buffer_[BufferSize] |  变长条目连续存放，每条 8 字节对齐
+------------------+
```

### 主要 API

| 方法 | 作用 |
|------|------|
| `uint64_t Append(TypeEnum, const void*, uint16_t len)` | 写入；返回条目 seq，0 表示失败 |
| `template<typename T> uint64_t Append(TypeEnum, const T&)` | T 必须 `trivially_copyable`，按 `sizeof(T)` 写入 |
| `bool ReadNext(WalEntryHeader&, void* data, size_t max_len)` | 读一条；自动跳过跨越边界的 skip 记录和 CRC 错误 |
| `size_t ReadUnconfirmed(vector<pair<Header, vector<char>>>&, size_t max_count)` | 从 `confirmed_pos` 开始扫一批未确认条目 |
| `void Confirm(uint64_t seq)` | 找到匹配 seq 并推进 `confirmed_pos` |
| `void ConfirmBatch(size_t count)` | 顺序确认 `count` 条有效条目 |
| `GetWritePos / GetReadPos / GetConfirmedPos / GetNextSeq / GetUnconfirmedBytes / GetTotalEntries / GetTotalBytes` | 状态查询 |
| `void Reset()` | 清零并重置 magic/version |
| `bool IsValid() const` | header/buffer 是否就绪 |

### 回绕处理

`Append` 检测到剩余空间不够时，在当前位置写一个「跳过记录」（`seq=0`、`type=0`、`data_len=remaining - sizeof(Header)`、`crc32=0`），把 `write_pos` 推进到缓冲区末尾，然后**递归调用**自己在 0 位置重新写入。`wrap_count` 自增。`ReadNext` / `ReadUnconfirmed` 检测到 `seq==0` 或 `type==0` 就跳过。

### 空间与序号语义

- `write_pos / read_pos / confirmed_pos` 都是**绝对递增**的 64 位整数；缓冲区偏移是 `pos % BufferSize`。
- 空间校验：`used = write_pos - confirmed_pos`，若 `used + entry_size > BufferSize` 则写失败（返回 0）。即**消费者不及时 Confirm 会导致生产者阻塞**。
- 写入的条目大小 = `AlignUp(sizeof(WalEntryHeader) + len, 8)`；超过 `MaxEntrySize` 直接失败。
- 时间戳来自 `zrt::DateTimeUTC().Epoch19()`（见 [time.md](time.md)），每次 `Append` 独立取值。

### 使用模式

```cpp
enum class EvType : uint16_t { kInvalid = 0, kTrade = 1, kQuote = 2 };

zrt::ShmRing<EvType> ring("/md/ring");

// 生产者
struct TradeEv { int64_t ts; double px; int qty; };
const uint64_t seq = ring.Append(EvType::kTrade, TradeEv{1, 65000.0, 10});

// 消费者
zrt::WalEntryHeader<EvType> h{};
char buf[4096];
if (ring.ReadNext(h, buf, sizeof(buf))) {
    if (h.type == EvType::kTrade) {
        auto* ev = reinterpret_cast<const TradeEv*>(buf);
        ring.Confirm(h.seq);
    }
}

// 批量恢复（备机）
std::vector<std::pair<zrt::WalEntryHeader<EvType>, std::vector<char>>> batch;
ring.ReadUnconfirmed(batch, /*max_count=*/1024);
```

### 注意

- **SPSC**：只假设单生产者单消费者；同侧并发需外部加锁。`confirmed_pos` 写侧用来检查可用空间，读侧用来截断已处理前缀，设计上由消费者单独推进，但 API 没有防并发。
- **0 不是合法 seq**：`next_seq` 从 1 开始，0 被用作跳过记录标记；`TypeEnum` 的 0 值也被保留给「无效/跳过」，用户枚举要从 1 开始。
- **CRC 保护**：条目 CRC 由 `WalEntry::UpdateCrc()` 写入，读侧 `ValidateCrc()` 失败就跳过。
- **持久化**：共享内存段名字不变时重启不清空；`Reset()` 或 magic 不匹配才会重建。

## 选型速查

| 需求 | 用 |
|------|----|
| 偶发单次下发一个 POD | `WriteShm / ReadShm` |
| 单线程高频读写 POD | `DirectShm` |
| 单写多读、需要原子快照 | `ShmCheckpoint` |
| SPSC、变长事件流、WAL 语义 | `ShmRing` |

跨进程真正持久化到磁盘用 [wal.md](wal.md) 的 `WalFileWriter`；`ShmRing` 只是在内存里维护未确认队列。
