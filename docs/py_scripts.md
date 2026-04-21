# Python 代码生成脚本

`src/zrtools/py_scripts/` 下的三个脚本，基于 [`CppHeaderParser`](https://pypi.org/project/CppHeaderParser/) 解析 C++ 头文件，生成对应的样板代码。**都是开发期工具，不参与运行时编译**。

| 脚本 | 类 | 产物 | 用途 |
|------|----|------|------|
| `create_describe.py` | `DescribeCreator` | `{core}_describe.h` | `BOOST_DESCRIBE_STRUCT` 声明，用于 boost.describe 的反射 |
| `create_mock.py` | `MockCreator` | `{core}_mock.h` | 生成 `Mock{ClassName}` 子类，使用 `MOCK_METHOD` 覆盖 public 方法 |
| `create_print_func.py` | `GenDumpFunc` | `{core}_dump.h` | 生成 `operator<<(std::ostream&, const T&)` 便于日志/调试 |

## 依赖

```bash
pip install CppHeaderParser
```

所有脚本都只依赖标准库 + `CppHeaderParser`，没有配置文件，不打包。

## 共同行为

- **读取编码**：先按 `utf8` 读，`UnicodeDecodeError` 自动回退 `gbk`。
- **解析步骤**：`CppHeaderParser.CppHeader(content, argType='string')`，然后遍历 `classes` → `properties['public']` / `methods['public']`（仅 public 成员）。
- **输出位置**：默认写到源文件所在目录；可以通过 `dst_dir` 参数改目标目录。
- **命名**：输出文件名 = `{core_name}_{describe|mock|dump}.h`，`core_name` 为去掉扩展名的头文件名。
- **换行**：写盘统一用 `newline='\n'`，规避 Windows 下 CRLF。

## `DescribeCreator`（`create_describe.py`）

```python
dc = DescribeCreator()
dc.create_describe(file_path, dst_dir="", ns="")
```

生成的内容形如：

```cpp
#pragma once
#include <boost/describe.hpp>
#include "orig.h"
#include <iostream>
// note：本文件由脚本自动生成

BOOST_DESCRIBE_STRUCT(Order, (BaseA,BaseB), (id,px,qty));
```

- 只描述**字段**（public properties）和直接基类。
- 不处理 method、template 参数、`private/protected`。
- 生成后需要 boost.describe 运行时库支持（`boost::describe::has_describe_members` 等）。

## `MockCreator`（`create_mock.py`）

```python
mc = MockCreator()
mc.create_mock(dir_name, base_name, dst_dir="", ns="")
# 注意：签名是 (dir_name, base_name)，和 describe/dump 的 (file_path) 不一致
```

生成：

```cpp
#pragma once
#include <gmock/gmock.h>
#include <iostream>
#include "iface.h"
// note：本文件由脚本自动生成

class MockIface : public Iface {
public:
    MOCK_METHOD(int, Foo, (int x, const std::string& s), (override));
    MOCK_METHOD(void, Bar, (), (const,override));
};
```

- 遍历 `methods['public']`，把 `const/override/final/virtual` 汇总到后缀数组。
- `virtual` 也会生成 `override`（通过 `i['override'] or i['virtual']` 组合）。
- 参数拼接：`"{type} {ns}::{name}"`；`ns` 通常为空，CppHeaderParser 在带命名空间限定时会填。
- 不处理模板成员函数、重载决议、默认参数；复杂签名要手工检查生成结果。

> 签名形式 `create_mock(dir_name, base_name, ...)` 与其它两个脚本不同，调用时注意顺序。

## `GenDumpFunc`（`create_print_func.py`）

```python
gdf = GenDumpFunc()
gdf.gen_dump_func(file_path, dst_dir="", ns="")
```

生成：

```cpp
#pragma once
#include "orig.h"
#include <iostream>

static std::ostream& operator<<(std::ostream& os, const ns::Order& st)
{
    os << "{"
    << dynamic_cast<const ns::Base&>(st) << ","
    << "id:" << st.id << ","
    << "px:" << st.px << ","
    << "qty:" << st.qty
    << "}";
    return os;
}
```

- 基类通过 `dynamic_cast<const Base&>(st)` 转发 `operator<<`（要求基类自己或更早的生成件也实现了 `<<`）。
- 数组字段（size > 1 且非 `char`）会展开 `for` 循环逐个打印，用 `memcmp(&empty, &empty, ...) == 0` 判同（相当于永远 `false`，**等同于不跳过空值**，实现上不过滤）。
- `char[N]` 被当作 C 字符串直接 `<<` 输出。
- 生成的是 `static` 函数放在头里，**多翻译单元包含会得到内部链接的副本**，放心多包含。

## 使用方式

三种脚本都是 `__main__` 里就地演示：

```bash
cd src/zrtools/py_scripts/
python create_describe.py       # 对 interface.h 生成 interface_describe.h
python create_mock.py           # 对 turtle.h    生成 turtle_mock.h
python create_print_func.py     # 对 interface.h 生成 interface_dump.h
```

工程里一般把脚本作为一次性工具：写完/修改完头文件后手动重跑，把产物签入仓库，不纳入构建链。如果想纳入 CMake，可加一个 `add_custom_command` 调 `python3 create_*.py`，产物放到 `${CMAKE_BINARY_DIR}` 再 `target_include_directories`。

## 局限 / 已知坑

- `CppHeaderParser` 对**模板、宏、C++17/20 新特性**（`concept`、`[[attribute]]`、`if constexpr`、`requires` 等）解析能力有限，复杂头文件往往漏掉成员或把类型解析错。对自动生成结果需要人工审校，必要时先跑预处理：`cpp -P header.h > flat.h` 再喂脚本。
- 只处理 `public`，`protected/private` 不生成。
- 数组字段仅按固定大小 `int array_size` 处理；模板数组 / `std::array<T, N>` 不识别。
- 所有生成器默认假设传入头路径可直接 `#include`；生成文件里会写 `#include "{base_name}"`，如果产物和源文件不同目录，需要调整 include 路径。
- 生成的文件头部有 `// note：本文件由脚本自动生成`，**不要手工改**；重跑会直接覆盖。
