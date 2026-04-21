# 文件模块

三代 API 并存，推荐新代码用 `zrt_file.h` 中的 `FileWriter` / `FileReader`。

| 文件 | 主类 | 状态 |
|------|------|------|
| `filesystem_compatible.h` | `namespace fs` 别名 | 公共基础 |
| `file.h` / `file.cpp` | `zrt::File`（fstream 包装）、旧 `zrt::FileWriter` | 旧版 |
| `cfile.h` / `cfile.cpp` | `zrt::cfile`（`FILE*` 包装） | 旧版 |
| `zrt_file.h` | `zrt::FileWriter` / `zrt::FileReader` | **推荐** |

> `file.h` 和 `zrt_file.h` 都定义 `namespace zrt::FileWriter`，**同时包含会报重定义**。二选一。

## filesystem_compatible.h

```cpp
#if __cplusplus >= 201703L
    namespace fs = std::filesystem;
#else
    namespace fs = std::experimental::filesystem;  // 需链接 -lstdc++fs
#endif
```

## zrt_file.h（推荐）

### `zrt::FileWriter`

- `FileWriter(file_path, append = false)`：只保存路径，不立即打开。
- `bool Exist()`：`fs::exists`。
- `bool EnsureDir()`：不存在则 `fs::create_directories`。
- `bool Open()`：幂等；内部先 `EnsureDir`，按 `append` 选 `ios::app` / `ios::trunc`。
- `bool WriteLine(const std::string&)`：懒打开 + 追加换行。
- `bool WriteFmt(const std::string& fmt_str, Args...)`：`fmt::format` 后 `WriteLine`。
- `bool SetHeader(fmt_str, Args...)`：**文件不存在时**才写首行，用于 CSV header 幂等初始化。
- `void Flush()`：转发 `std::ofstream::flush`。
- `const std::string& GetFilePath()`。

### `zrt::FileReader`

- `FileReader(file_path)`。
- `bool Exists()` / `bool Open()`（懒）。
- `std::string Read()`：`rdbuf()` 整读。
- `bool ForEachLine(std::function<bool(const std::string&)>)`：按行回调，回调返回 `false` 即停止。
- `void Rewind()`：`clear() + seekg(0)`，重新从头读。

### 典型用法

```cpp
#include "zrtools/zrt_file.h"

zrt::FileWriter w("/data/trades.csv", /*append=*/true);
w.SetHeader("symbol,px,qty");                // 首次落盘时写表头
w.WriteFmt("{},{:.2f},{}", "BTCUSDT", 65000.12, 0.1);

zrt::FileReader r("/data/trades.csv");
r.ForEachLine([](const std::string& line){
    SPDLOG_INFO("line: {}", line);
    return true;  // 继续
});
```

## file.h / file.cpp（旧版）

### `zrt::File`

`std::fstream` 的简单 RAII 包装。

```cpp
explicit File(const std::string& filename,
              std::ios_base::openmode mode = std::ios_base::in,
              bool ensure_dir = true);
```

- 构造时打开失败会 `throw std::runtime_error("file init failed:" + filename)`。
- `bool isOpen() const`。
- `bool read(std::string& content)` / `std::string read()`：按行读取后拼回（每行追加 `"\n"`）。
- `bool write(const std::string& content)`：`file_ << content`。

> **`ensure_dir` 目前是占位符**，构造函数里 `if (ensure_dir) {}` 分支为空。需要自动建目录请改用 `zrt_file.h::FileWriter`。

### `zrt::FileWriter`（旧版）

与 `zrt_file.h::FileWriter` 同名。功能子集：无 `append`，无 `EnsureDir`，无 `SetHeader`。

- `Open(file_path)` 显式打开；首次 `WriteLine` 会自动尝试用构造时路径打开。
- `WriteLine` / `WriteFmt` / `Flush`。

## cfile.h / cfile.cpp

`FILE*` 包装，保留主要是为了兼容 `fprintf` 风格。

```cpp
explicit cfile(const std::string& filename, const std::string& mode = "r", bool ensure_dir = true);
```

- 构造或 `open` 失败抛 `std::runtime_error`。
- `bool isOpen() const`。
- `void sync() const`：`fflush`。
- 读：
  - `bool read(char* content, size_t size)`：`fread` 固定字节（大小不匹配抛异常）。
  - `bool read(std::string& content)`：`fseek SEEK_END` 得到文件大小 → `resize` → 一次性读满。
  - `std::string read()`。
- 写：
  - `bool write(const std::string&)`：`fputs`。
  - `int write(const char* format, ...)`：`vfprintf` 风格，返回写入字节数。

> 同样 `ensure_dir` 实现为空；不会自动建目录。
> `fread` 参数顺序是 `fread(buf, size, 1, fp)`，若用小文件需注意结果含义。

## 建议

- **新代码**：统一用 `zrt_file.h`。需要打印到日志时配合 `spdlog`；CSV/日志滚动类场景 `SetHeader` + `WriteFmt` 够用。
- **跨平台二进制读写 / 需 `fprintf`**：暂用 `cfile`，但要注意 `ensure_dir` 占位。
- **大文件流式处理**：直接 `FileReader::ForEachLine`。
- **创建目录**：`FileWriter::EnsureDir()` 已经覆盖，或直接 `fs::create_directories`。
