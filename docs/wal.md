# WAL 模块

Write-Ahead Log：把业务事件以二进制条目形式先记录下来，用于崩溃恢复和主备同步。两层：

- `zrt_wal_entry.h`：通用条目定义 + 缓冲区构建器（纯内存结构，无 I/O）。
- `zrt_wal_file.h`：文件持久化 + 旋转扫描 + 读取重放。

共享内存版本见 [shm.md](shm.md) 的 `ShmRing`（同样复用 `WalEntryHeader`）。

## zrt_wal_entry.h

### `WalEntryHeader<TypeEnum>`（24 B，固定）

```cpp
template<typename TypeEnum>
struct WalEntryHeader {
    uint64_t seq;           // 单调递增序号
    uint64_t timestamp_ns;  // 纳秒时间戳
    TypeEnum type;          // 条目类型
    uint16_t data_len;      // 不含头
    uint32_t crc32;         // 数据 CRC32

    static uint32_t CalcCrc32(const void* data, size_t len);  // 走 zrt::Crc32::Calc
};
```

- `TypeEnum` 必须是 `enum class : uint16_t`（`static_assert` 要求 `is_enum` + `sizeof == 2`）。
- `constexpr size_t kWalEntryHeaderSize<TypeEnum> = 24`。

### `WalEntry<TypeEnum>`（柔性数组）

```cpp
template<typename TypeEnum>
struct WalEntry {
    WalEntryHeader<TypeEnum> header;
    char data[0];           // 柔性数组成员

    size_t TotalSize() const;           // header + data_len
    bool   ValidateCrc() const;         // data_len==0 时要求 crc32==0
    void   UpdateCrc();                 // 按 data_len 重新算
    const void* GetData() const;
    void*       GetData();
    uint16_t    GetDataLen() const;

    template<typename T> const T* GetDataAs() const;   // T 需 trivially_copyable
    template<typename T> T*       GetDataAs();
};
```

> `char data[0]` 是 GCC 扩展；把它当成零长度柔性数组用，不能 `new WalEntry<E>` 而要放在已有缓冲区上 `reinterpret_cast`。

### `WalEntryBuilder<TypeEnum, MaxSize=4096>`

在栈上的固定大小缓冲区中拼条目：

```cpp
template<typename TypeEnum, size_t MaxSize = 4096>
class WalEntryBuilder {
    bool Build(uint64_t seq, uint64_t ts_ns, TypeEnum type, const void* data, uint16_t len);
    template<typename T> bool Build(uint64_t seq, uint64_t ts_ns, TypeEnum type, const T& data);

    const WalEntry<TypeEnum>* GetEntry() const;
    WalEntry<TypeEnum>*       GetEntry();
    size_t      GetSize() const;
    const char* GetBuffer() const;
    void        Reset();
};
```

- 缓冲区 `alignas(8) char buffer_[MaxSize]`。
- `Build` 超过 `MaxSize` 返回 `false`；数据拷贝完成后自动 `UpdateCrc()`。
- `Reset()` 会 `memset` 整个缓冲。

### `AlignUp`

```cpp
inline size_t AlignUp(size_t size, size_t alignment);  // (size + a - 1) & ~(a - 1)
```

WAL/ShmRing 内部的条目大小统一按 8 字节对齐。

### 典型用法

```cpp
enum class EvType : uint16_t { kInvalid = 0, kOrder = 1, kTrade = 2 };

zrt::WalEntryBuilder<EvType> builder;
struct OrderEv { int64_t id; double px; int qty; };

builder.Build(/*seq=*/1, zrt::get_epoch19(), EvType::kOrder,
              OrderEv{100, 65000.0, 10});

const auto* entry = builder.GetEntry();
assert(entry->ValidateCrc());
const auto* ev = entry->GetDataAs<OrderEv>();
```

## zrt_wal_file.h

### `WalFileHeader`（64 B，固定）

```cpp
struct WalFileHeader {
    uint32_t magic {0x5A524657};   // "ZRFW" (ZRT File WAL)
    uint32_t version {1};
    uint64_t start_seq {0};        // 首条记录序号
    uint64_t entry_count {0};      // 条目数（Close 时回写）
    uint64_t create_time_ns {0};
    char     reserved[32] {};
};
static_assert(sizeof(WalFileHeader) == 64, ...);
```

- **兼容旧 magic** `0x57414C46`（"WALF"，gtrade 遗留格式）。读路径同时接受这两种。

### `WalFileWriter<TypeEnum>`

```cpp
using TimestampFunc = std::function<uint64_t()>;

WalFileWriter(const std::string& dir,
              const std::string& prefix = "wal",
              TimestampFunc ts_fn = nullptr);   // 默认 DateTimeUTC().Epoch19()

bool     Init();                                // 建目录 + 扫 *.wal + 打开新文件
uint64_t Append(TypeEnum type, const void* data, uint16_t len);   // 返回 seq，0 失败
template<typename T> uint64_t Append(TypeEnum type, const T& data);
void     Sync();                                // ofstream::flush
void     Close();                               // 回写 header 后关闭
uint64_t GetCurrentSeq() const;
void     SetSyncInterval(size_t n);             // 默认 100 条 flush 一次
size_t   GetTotalSize() const;                  // 目录下所有 .wal 字节和
size_t   TruncateBefore(uint64_t seq) const;    // 删除整段 end_seq<seq 的旧文件
```

关键行为：

1. **文件命名**：`{prefix}_{start_seq}.wal`，如 `wal_1.wal`、`wal_101.wal`。`Init()` 扫描 `dir_` 下所有 `*.wal`，用 `ParseSeqFromFilename` 找到文件名里 `_` 和 `.` 之间的数字，再用 `GetLastSeqInFile` 读内容求出文件末尾序号，取全局最大作为 `current_seq_`；然后创建 `{current_seq_+1}.wal` 开始写。
2. **并发**：所有写路径（`Append`、`Sync`、`Close`）共用 `std::mutex mutex_`。
3. **刷盘节奏**：默认每 100 条 `flush`，可用 `SetSyncInterval` 调整；调用方需要强一致时显式 `Sync()`。
4. **序号 0 是终止标记**：`GetLastSeqInFile` 读到 `entry_header.seq == 0` 即停止；`Append` 的 `seq` 从 `++current_seq_` 开始，必然 ≥ 1。
5. **`TruncateBefore`**：按每个文件的 `GetLastSeqInFile` 决定是否整文件删掉；**不会删 `current_file_`**，也不会半删。
6. **`Close`** 会 `seekp(0)` 把 `start_seq/entry_count/create_time_ns` 回写 header，再回到末尾。

> `Init` 里 `max_seq` 的两行赋值
> ```cpp
> max_seq = seq;
> max_seq = GetLastSeqInFile(entry.path().string());
> ```
> 是把「按文件名候选」立刻覆盖成「按文件内容的末尾序号」，第一行其实没用处，留意代码意图而不是行为。

### `WalFileReader<TypeEnum>`

```cpp
WalFileReader(const std::string& dir, const std::string& prefix = "wal");

size_t ReadAfter(uint64_t after_seq,
                 std::vector<std::pair<WalEntryHeader<TypeEnum>, std::vector<char>>>& out,
                 size_t max_count = 0);       // 0 表示不限
```

- 按文件名排序依次扫描；magic 不匹配直接 `continue`。
- 每读一个条目头：`seq > after_seq` 才追加到 `out`；否则仅 `seekg(data_len, cur)` 跳过。
- `max_count > 0` 时读够就返回。
- 读到 `seq == 0` 或流异常结束当前文件，继续下一个。
- **不会**校验 CRC；重放方需要时自己调用 `WalEntryHeader::CalcCrc32` 对比。

### 典型用法

```cpp
enum class EvType : uint16_t { kInvalid = 0, kOrder = 1, kTrade = 2 };

zrt::WalFileWriter<EvType> wal("/data/wal/strategy1", "wal");
wal.Init();
wal.Append(EvType::kOrder, OrderEv{100, 65000.0, 10});
wal.Sync();

zrt::WalFileReader<EvType> rd("/data/wal/strategy1", "wal");
std::vector<std::pair<zrt::WalEntryHeader<EvType>, std::vector<char>>> batch;
rd.ReadAfter(/*after_seq=*/0, batch);
for (auto& [h, payload] : batch) {
    if (h.type == EvType::kOrder) {
        const auto* ev = reinterpret_cast<const OrderEv*>(payload.data());
    }
}
```

## 与 ShmRing 的搭配

常见拓扑：

```
业务线程 -> ShmRing -> 后台持久化线程 -> WalFileWriter -> 磁盘
                \
                 \-> 备机 ShmRing / WAL 回放
```

- `ShmRing::Append` 产生序号并入环；
- 持久化线程 `ReadUnconfirmed` 取一批 → `WalFileWriter::Append` 转写文件 → `ShmRing::Confirm(seq)`；
- 崩溃恢复：`WalFileReader::ReadAfter(last_confirmed_seq, ...)` 补齐。

## 注意

- `TypeEnum` 的 0 值约定为「无效/跳过」（`ShmRing` 依赖它标识回绕填充；`WalFileWriter` 也以 `seq==0` 做终止），定义枚举时请把真实业务类型从 1 起编号。
- 当前实现不做**文件级 checksum**，条目 CRC 只护数据体，头部与 data_len 写坏会导致解析出错。
- `WalFileWriter` 目前不会**自动滚动**到新文件，`Init` 只在启动时切一次。需要按大小/时间滚动请自行在外部定期 `Close()` + 重建实例。
- 路径 API 基于 `std::filesystem`，要求 C++17。
