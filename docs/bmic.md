# Boost MultiIndex Container (BMIC) 外观

`zrt_bmic*.h` 把 Boost.MultiIndex 的冗长声明压成一行宏，并提供一个 `zrt::BMIC<Container, Val>` 外观类封装常见 CRUD。

三个文件：
- `zrt_bmic.h`：容器宏、索引宏基础、`BMIC` 外观、自由函数 API。
- `zrt_bmic_hashed.h`：1~9 字段的 `hashed_*` 复合索引宏。
- `zrt_bmic_ordered.h`：1~9 字段的 `ordered_*` 复合索引宏。

## 基础宏（zrt_bmic.h）

| 宏 | 展开 |
|----|------|
| `ZRT_BMI_SEQ_INDEX` | `boost::multi_index::sequenced<>` |
| `ZRT_BMIC(cls, ...)` | 内联 `multi_index_container<cls, indexed_by<...>>` |
| `ZRT_DECLARE_BMIC(name, cls, ...)` | `using name = multi_index_container<cls, indexed_by<...>>;` |

## Hashed 索引（zrt_bmic_hashed.h）

### 单字段（自动 Tag）
```cpp
ZRT_BMI_HASHED_INDEX(unique,     MyCls, id)       // Tag = Tag_id
ZRT_BMI_HASHED_UNIQUE_INDEX(MyCls, id)            // 同上
ZRT_BMI_HASHED_NON_UNIQUE_INDEX(MyCls, kind)
```

### 多字段复合（自定义 Tag 或约定 `TagPrimeKey`）
```cpp
ZRT_BMI_HASHED(2, unique, TagPrimeKey, MyCls, a, b)
ZRT_BMI_HASHED(3, non_unique, MyTag, MyCls, a, b, c)
```

复合键自动使用 `zrt::my_hash<decltype(MyCls::x)>` 和 `zrt::my_equal<decltype(MyCls::x)>` 组合，因此 `char[N]` 字段也能按 `std::string` 语义哈希/判等。支持到 9 字段。

### 声明完整容器
```cpp
using C = ZRT_HASH_BMIC(MyCls, id);            // 单字段 hash_unique
using C2 = ZRT_HASH_BMIC_3(MyCls, a, b, c);    // 三字段复合 hash_unique，Tag = TagPrimeKey
using C3 = ZRT_SEQ_HASH_BMIC(MyCls, id);       // 同时带 sequenced + hash_unique
```

### 按成员函数索引
```cpp
ZRT_BMI_HASHED_UNIQUE_FUN_INDEX(MyCls, int, GetPk)   // 走 MyCls::GetPk() const
```

## Ordered 索引（zrt_bmic_ordered.h）

与 Hashed 平行，把 `hashed_*` 换成 `ordered_*`，把 `composite_key_hash / _equal_to` 换成 `composite_key_compare` + `zrt::my_less`。常用入口：

```cpp
ZRT_BMI_ORDERED_INDEX(unique, MyCls, ts)           // Tag_ts
ZRT_BMI_ORDERED_UNIQUE_INDEX(MyCls, ts)            // 同上
ZRT_BMI_ORDERED_NON_UNIQUE_INDEX(MyCls, kind)

ZRT_BMI_ORDERED(2, unique, TagTsKind, MyCls, ts, kind)
ZRT_ORDERED_BMIC(3, MyCls, a, b, c)                 // 三字段复合 ordered_unique
```

支持 1~9 字段的复合索引。

## `zrt::BMIC<Container, Val>` 外观类

把常用 CRUD 包成普通方法，避免散落的 `container.get<Tag>()` 模板冗余。

```cpp
template<typename Container, typename Val>
class BMIC {
    void Set(const Val&);

    template<Index, Key> bool Remove(key);
    template<Index, Key> void RemoveRange(key);                 // equal_range
    template<Index, Key> void RemoveRange(lo, hi);              // ordered 专用 range()
    template<Index, Key> bool Update(key, const Val&);          // 不存在则 emplace
    template<Index, Key> int  UpdateRange(key, std::function<Val(Val)>); // ordered 专用

    bool IsEmpty();
    template<Index, Key> bool Has(key);

    template<Index, Key> bool TryCopy(key, Val&);
    template<int N, Key>  bool TryCopy(key, Val&);              // 走 index 编号
    template<Index, Key> Val  TryCopy(key);                     // 找不到返回 Val{}
    template<Index, Key> boost::optional<Val> Copy(key);

    template<Index> auto CBegin() const;                        // const_iterator
    template<Index> auto Begin()  const;
    template<Index> int ForEach(std::function<void(const Val&)>); // 遍历整体
    template<Index, Key> int ForEach(key, fn);                    // equal_range
    template<Index, Key> int ForEach(lo, hi, fn);                 // range (ordered)

    size_t GetSize();
    template<Index> std::string Dump();                         // "[v1,v2,...]"
};
```

注意：
- `Update` 找不到 key 时会直接 `emplace(val)`，相当于 upsert 语义；调用方想区分时要用返回值。
- `RemoveRange(key)` 适用于 `equal_range`（hashed/ordered non-unique）；`RemoveRange(lo, hi)` 仅在 `ordered_*` 索引上生效（`range()`）。
- `UpdateRange` / `ForEach(lo, hi, ...)` 同理需要 ordered 索引。
- `Dump` 依赖 `zrt::to_str(Val)`（来自 `stl_dump.h`），所以 `Val` 需要有 `operator<<`。
- `GetSize()` / `IsEmpty()` 等没加 const。

## 自由函数 API

不想走外观时，直接用：

```cpp
namespace zrt {
    template<Container, Val> void  SetBmic(c, v);
    template<Index, Container, Key, Val> void UpdateBmic(c, k, v);

    template<Index, Container, Key, Val> bool TryCopyBmicVal(c, k, v);
    template<int   N, Container, Key, Val> bool TryCopyBmicVal(c, k, v);
    template<Index, Val, Container, Key>  Val  TryCopyBmicVal(c, k);
    template<Index, Val, Container, Key>  boost::optional<Val> CopyBmicVal(c, k);

    template<Index, Container, Func> void ForEachBmicConstVal(c, func);
    template<Index, Container, Func> void ForEachBmicVal(c, func);
}
```

## 完整示例

```cpp
#include "zrtools/zrt_bmic_hashed.h"
#include "zrtools/zrt_bmic_ordered.h"

struct Order {
    int64_t id;
    std::string sym;
    double px;
    int64_t ts;
};

using OrderContainer = ZRT_BMIC(Order,
    ZRT_BMI_HASHED_UNIQUE_INDEX(Order, id),                    // pk
    ZRT_BMI_ORDERED_NON_UNIQUE_INDEX(Order, ts),               // 时间
    ZRT_BMI_HASHED_NON_UNIQUE_2_PARAM_INDEX(TagSymTs, Order, sym, ts)
);

zrt::BMIC<OrderContainer, Order> store;
store.Set({1, "BTC", 65000, 1});
store.Set({2, "ETH", 3200,  2});

Order out;
if (store.TryCopy<Tag_id>(1, out)) {
    SPDLOG_INFO("got {} @ {}", out.sym, out.px);
}

store.ForEach<Tag_ts>([](const Order& o){ SPDLOG_INFO("{} -> {}", o.ts, o.id); });
```

## 注意事项

- 所有 Tag 是 `struct TagName`（在外部命名空间），含有相同字段的容器请自定义 Tag 避免冲突。自动生成的 `Tag_member` 也是类型，不能重复声明。
- `TagPrimeKey` 是多个宏默认的 Tag，别在同一翻译单元同时有两个容器都把它当主键。
- 复合索引使用 `zrt::my_hash` / `zrt::my_equal` / `zrt::my_less`，和标准库 `std::hash/equal_to/less` 的语义可能略有差异（参考 [math_compare.md](math_compare.md)）。
