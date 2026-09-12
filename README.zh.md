# Program Analysis for Vibe Coding

*English (default): [README.md](README.md) · [CONCEPT-MAP.md](CONCEPT-MAP.md) · [WORKFLOW.md](WORKFLOW.md)*

C++ 精简版 demo，18 个文件。

把《Program Analysis》01–09 讲的概念落成一个可运行的门禁，专门针对 AI agent 生成的代码。
理论背景见 [`docs/ai-code-under-control.zh.md`](docs/ai-code-under-control.zh.md)（[English](docs/ai-code-under-control.md)）。18 个文件。

```
cmake -S . -B build -G Ninja && cmake --build build
./build/gate check                        # 对当前代码跑门禁
./build/gate demo                         # 六个场景：把 agent 的候选产出依次装进来
./build/gate demo 1                       # 只跑第 1 个
./build/gate affected app/core/users.hpp  # worklist：改了它要重查什么
```

只依赖 g++ 和 CMake。装了 clang-tidy 会自动接入，没装标 ○。

## 文件

```
app/core/billing.hpp/.cpp      trusted    settings()（隐式流源头）+ charge()/ledger()
app/core/users.hpp             trusted    INVARIANT 块 + 声明
app/core/users.cpp             trusted    缓存 + get_user + invalidate（读侧）
app/core/users_write.cpp       trusted    update_user（写侧）—— 与读侧的跨文件不变式
app/features/dedupe.hpp        untrusted  只有 α；spec 在 specs/
app/features/promo.cpp         untrusted  非干扰场景的目标文件
specs/specs.cpp                外化的 spec：lower（示例）/ upper（随机性质 + 收缩反例）
specs/golden.cpp               非干扰观测器，构建两次（golden_core / golden_full）
gate/gate.cpp                  门禁：顶部是抽象域声明区，然后是检查、报告、worklist、demo
candidates/*                   五份"agent 交回来的代码"，每份故意错在一处，demo 用
CLAUDE.md                      给 agent 的规则 + 每条的讲义原因
```

## 概念 → 代码

完整的四列对照表（讲义概念 · 讲义里说什么 · 映射到门禁 · 代码位置）在 **[CONCEPT-MAP.zh.md](CONCEPT-MAP.zh.md)**，
按讲义 01–09 分章，末尾有三条承重公式和 `f(γ(m)) ⊑ γ(f#(m))`、`l ⊑ AC(l,u) ∧ u ⋢ AC(l,u)` 的展开说明。

## 报告怎么读

```
UPPER  约束 —— 绝不能        机械、可无人值守
LOWER  示例 —— 必须能        证明做的是要求的事
HYPER  超性质 —— 两次运行比对

上界 ✓ 下界 ✓ → 可合并
上界 ✓ 下界 ✗ → 没越界，但没做到要求的事（等人审 / 再生成）
上界 ✗ 下界 ✓ → 做到了要求的事，但越界了（拒绝，不需要人审）   ← 把人从瓶颈里挪出来的那一格
上界 ✗ 下界 ✗ → 拒绝
```

## 六个场景（`gate demo`）

| # | 候选 | 抓住它的 | 讲义 |
|---|---|---|---|
| 0 | 基线 | 全过 | 这是 I |
| 1 | `dedupe` 用 `std::set` | upper（反例 `["abb", "a"]`） | 07 只有下界时的最小程序 |
| 2 | `dedupe` + 等价的 `unique` | reduce | 05 冗余表示 = 腐烂 |
| 3 | features `#include` billing 改全局取整 | arch | 03 没写下的默契对 AI 不存在 |
| 4 | features 前置声明改全局取整 | noninterf（arch 是盲的） | 09 隐式流 |
| 5 | `update_user` 忘了 invalidate | lower（跨文件测试） | 07 复合 ≠ 复合的最佳 |

`gate demo` 跑完会还原文件。想亲眼看：`cp candidates/dedupe_set.hpp app/features/dedupe.hpp && ./build/gate check app/features/dedupe.hpp`，再用 git 或手动还原。

## License

MIT，见 [LICENSE](LICENSE)。
