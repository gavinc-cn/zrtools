# CRC32 & Snowflake 模块

## zrt_crc32.h

`zrt::Crc32`：IEEE 802.3 多项式（`0xEDB88320`）的 CRC32，查表法，与 `zlib / gzip` 一致。

| 静态方法 | 语义 |
|----------|------|
| `uint32_t Calc(const void* data, size_t len)` | 一次性计算 |
| `uint32_t Update(uint32_t crc, const void* data, size_t len)` | 增量计算。`crc == 0` 视为首次调用。 |
| `bool Verify(const void* data, size_t len, uint32_t expected)` | `Calc(...) == expected` |

表 `kCrcTable[256]` 是 `static constexpr`，编译期生成，无运行时初始化开销。

### 与 WAL 的集成

`zrt_wal_entry.h::WalEntryHeader::CalcCrc32` 直接调用 `Crc32::Calc`；任何新的校验和使用点建议统一走这个类以保证一致。

### 性能注意

- 表驱动，单字节处理 ~300MB/s 量级；对 GB 级吞吐可换成 SSE4.2 CRC32C 或 PCLMULQDQ 指令。
- `Update` 将中间 `crc` 再次取反处理，**不能与其他库的流式 CRC（如 `zlib` 的 `crc32(0, buf, len)`）混用**，必须从 0 起一路 `Update`。

## zrt_snowflake.h

`zrt::Snowflake<...>`：可配置位宽的雪花 ID 生成器，带三档时钟回拨策略。

### 模板参数

| 参数 | 默认 | 含义 |
|------|------|------|
| `ServerIdBits` | 8 | 服务器 ID 位数，范围 `[1, 16]` |
| `SequenceBits` | 14 | 同毫秒内序列号位数，范围 `[1, 22]` |
| `TimestampBits` | 41 | 时间戳位数，范围 `[1, 50]` |
| `ToleranceMs` | 5 | 小幅回拨容忍（复用上次时间戳） |
| `WaitThresholdMs` | 10000 | 等待阈值（`yield` 等回拨结束） |
| `ErrorThresholdMs` | 10000 | 报错阈值（大于等于则抛异常） |

`static_assert` 强制 `ServerIdBits + SequenceBits + TimestampBits == 63`（第 64 位是符号位）。

### 常用别名

| 别名 | 配置 | 适用场景 |
|------|------|----------|
| `SnowflakeStandardV2` | `<8, 14, 41, 5, 10000, 10000>` | 默认 |
| `SnowflakeStrictV2` | `<8, 14, 41, 0, 100, 100>` | 严格模式，不容忍 |
| `SnowflakeTolerantV2` | `<8, 14, 41, 10, 60000, 60000>` | 宽容 |
| `SnowflakeHighAvailability` | `<8, 14, 41, 5, 30000, UINT64_MAX>` | 永不报错 |
| `SnowflakeLargeV2` | `<10, 12, 41, ...>` | 1024 台服务器 |
| `SnowflakeHighThroughputV2` | `<8, 16, 39, ...>` | 每毫秒 65536 个 ID |

### 位布局

```
[sign:1][timestamp:T][server_id:S][sequence:Q]
                    ^             ^
                    TIMESTAMP_SHIFT SERVER_ID_SHIFT
```

- `SERVER_ID_SHIFT = SequenceBits`
- `TIMESTAMP_SHIFT = SequenceBits + ServerIdBits`
- `MAX_SERVER_ID = (1 << ServerIdBits) - 1`
- `MAX_SEQUENCE = (1 << SequenceBits) - 1`
- `MAX_TIMESTAMP = (1 << TimestampBits) - 1`
- `DEFAULT_EPOCH = 1704067200000ULL`（2024-01-01 00:00:00 UTC）

### API

```cpp
explicit Snowflake(uint64_t server_id, uint64_t epoch_ms = DEFAULT_EPOCH);
uint64_t nextId();                 // 无锁 CAS
ParsedId parseId(uint64_t id) const;
double   getMaxYears() const;      // 从 epoch 起可用年数
void     printConfig() const;
void     printStats() const;
void     resetStats();
```

`ParsedId { timestamp_ms, server_id, sequence }`。

### 生成流程

1. 读原子状态 `state_ = (timestamp << SEQUENCE_BITS) | sequence`。
2. 取当前毫秒 `now`，与 `last_timestamp` 对比：
   - `now < last`：按 `drift` 走**三档**策略。
   - `now == last`：`sequence + 1`，溢出则 `waitNextMillis`。
   - `now > last`：`sequence = 0`。
3. 组装 `new_state`，`compare_exchange_weak` 成功则返回 ID，失败重试。

### 时钟回拨三档

| drift 范围 | 行为 |
|------------|------|
| `< ToleranceMs` | 容忍：继续用 `last_timestamp`，每 100 次 TRACE 一次 |
| `[ToleranceMs, WaitThresholdMs)` | `yield` 等待 |
| `[WaitThresholdMs, ErrorThresholdMs)` | ERROR 日志，`sleep 10ms` 继续等 |
| `>= ErrorThresholdMs` | CRITICAL + `throw std::runtime_error` |

> `WaitThresholdMs == ErrorThresholdMs` 时不会进入「严重警告」分支；`ErrorThresholdMs == UINT64_MAX` 时等同于永不抛异常（高可用配置）。

### 异常

- 构造：`server_id > MAX_SERVER_ID` → `std::invalid_argument`。
- `nextId()`：时间戳溢出（到期） → `std::runtime_error("Timestamp overflow!")`。
- `nextId()`：drift 超过 `ErrorThresholdMs` → `std::runtime_error("Clock moved backwards by X ms...")`。

### 使用示例

```cpp
zrt::SnowflakeStandardV2 gen(/*server_id=*/1);
for (int i = 0; i < 10; ++i) {
    const uint64_t id = gen.nextId();
    const auto p = gen.parseId(id);
    SPDLOG_INFO("id={} ts={} srv={} seq={}", id, p.timestamp_ms, p.server_id, p.sequence);
}
gen.printStats();
```

### 注意事项

- 构造器内 `SPDLOG_INFO` 会输出约 11 行初始化日志，`printConfig/printStats` 各自再打印；在高频创建时谨慎使用。
- `nextId()` 在热路径会退化成 spin（CAS 失败重试），多生产者情况下单生成器吞吐量上限约等于 `MAX_SEQUENCE + 1` 条/毫秒。需要更高吞吐考虑分片（不同 `server_id`）。
- `state_` 只在单个 `Snowflake` 实例内保证原子；跨进程共享生成器请用 `shm + SeqLock` 等外部协调机制。
