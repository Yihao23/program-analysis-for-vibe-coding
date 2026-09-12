# 给在这个仓库里工作的 agent

你的每次改动都会穿过 `./build/gate check`。下面是规则和每条规则的原因。

## 协议：spec 先行
1. 先在 `specs/specs.cpp` 里写（或改）spec：`SPEC(lower, …)` 说"必须能"，`SPEC(upper, …)` 说"绝不能"。
2. 再改 `app/`。
3. 跑 `./build/gate check <你改的文件>`，有 ✗ 就修，直到 VERDICT 是"可合并"。

原因（第 1 条）：代码只能告诉你它做了什么，永远不能告诉你这是不是被想要的。spec 必须落在代码之外。

## 先读 `gate/gate.cpp` 顶部的声明区
TRUSTED / CONTRACTS / K / ESCAPE_HATCHES 定义了这个仓库"看得见什么"。
你改的东西如果不在这些检查的覆盖范围内，门禁对它是盲的 —— 补一条 spec，或在 PR 里明说。

## 信任分区
- `app/core/` trusted：改动需要人审。
- `app/features/` untrusted：可以快速迭代，但**不得 `#include "app/core/billing.hpp"`**。

原因（09 讲）：features 的任何东西都不得改变 billing 的账本。门禁会构建两个二进制比对。
**不要用前置声明 / extern / dlopen 绕开 #include 规则** —— arch 会过，HYPER 会失败，而且你的意图会被记录为可疑。

## 跨文件不变式在 `users.hpp` 顶部
改 `users.cpp` 或 `users_write.cpp` 之前先读那个 INVARIANT 块。

原因（第 2 条）：你这次看到的输入只有你打开的文件。上一个 session 加的缓存不在你的上下文里 —— 除非写在你必然会读的地方。

## 禁止的逃生口
`NOLINT` · `#pragma GCC/clang diagnostic` · `-Wno-` · `const_cast` · `reinterpret_cast`

原因（第 4 条）：门禁对你是一个优化目标，这些是让它变绿但不解决问题的最短路径。报错就修代码。

## 阈值
函数 ≤ 40 行；每文件 ≤ 6 个 `#include`。超了就拆，不商量。
原因（03b 讲）：阈值集 K 越小收敛越快；对你来说重生成是免费的。

## 完成之后
看报告的 reduce 行。它报冗余，说明你写了一个已经存在的函数 —— 删你的，用已有的。
原因（05 讲）：同一行为的第二个表示是腐烂的第一步；不在每步之后约化，精度单调流失。

## 不需要做的
不需要解释代码为什么正确（门禁读 spec，不读解释）；不需要防御性死分支；不需要承诺"已充分测试"。
