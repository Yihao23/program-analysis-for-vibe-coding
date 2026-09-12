# 用程序分析的视角控制 AI 生成的代码

> 灵感来源：Program Analysis 课程讲义 01–09（Jan Reineke, Saarland University）
> 数据流分析 · 抽象解释 · Widening/Narrowing · SMT · 符号抽象 · 符号/Concolic 执行 · 信息流分析

*English version: [ai-code-under-control.md](ai-code-under-control.md)*

---

## 核心论点

讲义里没有任何东西是 AI 专属的 —— 它讲的是"程序"，不管谁写的。
但 AI 生成代码改变了几个关键参数，使得这些原本存在的概念从"边缘情况"变成了"默认情况"。

```
   第 1 条：AI 让所有代码一出生就是遗留代码
            ⟹ spec 必须外化，α(code) 与 spec 的比对必须由机器做，不能靠作者
   第 7 条：AI 让严格分析的误报变得几乎免费
            ⟹ 机器做这个比对的代价终于付得起

   两件事同时发生。
   程序分析这门学科 50 年来第一次同时拿到了"必要"和"可行"。
```

其余五条（2–6）是这个论点在流程上的展开。

---

## 背景：讲义给出的思维框架

### Rice 定理（01 讲）："对"不是一个可检查的性质

```
  ✗  "这段代码是对的吗"            ← 不可判定，也不可定义
  ✓  "y 在 print 处能否为负"        ← 具体性质，可以近似
  ✓  "这个模块的公开接口有没有变"
```

第一步不是"设置测试"，而是把关心的东西写成可检查的性质列表。

### 归纳不变式（01/02 讲）：不检查每一步，检查"每一步都保持什么"

```
   Init ⊆ I              初始状态在 I 里
   Next(I) ⊆ I           从 I 走一步仍在 I 里
   I ∩ Bad = ∅           I 里没有坏状态

   Init = 当前代码库    Next = AI 的一次修改
   I    = 健康状态      Bad  = 不能接受的状态
```

控制一个快速变化的系统，不是靠看得比它快，而是靠让它每一步都必须穿过同一道门。

### 静态 vs 动态（01/08 讲）：两侧夹击

```
                   不漏 bug     不误报    分析必停
   静态分析(过近似)    ✓          ✗         ✓
   测试(欠近似)        ✗          ✓         ✓
   完美判定            ✓          ✓         ✗   ← Rice 说不存在

   下侧（欠近似）：测试、示例、属性测试     → "至少能做到这些"
   上侧（过近似）：类型、lint、架构约束     → "绝不会做那些"
```

### 传递函数与"最佳"（02/04/07 讲）

传递函数 `f_a : D → D` 是一条语句对抽象值的作用。最佳传递函数 `f# = α ∘ f ∘ γ`
是给定抽象域下最精确的那个。但**最佳是相对于域说的** —— swap 例子里每一步都最佳，
整体却从 `[1,5]` 爆炸到 `[−104,204]`，因为区间域装不下 `x′ = x − y` 这条关系。

---

## 七条差异：AI 代码 vs 人写代码

每条按"结构性差异 → 具体场景 → 讲义概念 → 做法"展开。

---

### 1. α(code) 是写作的副产品，AI 代码没有

**结构性差异**

程序分析这门学科诞生的场景是"没有作者可问的代码"：遗留系统、二进制、第三方库。
人写代码时，写的过程本身在作者脑中生成了抽象（不变式、边界、设计理由）—— 这是免费副产品。
AI 生成的代码，这个副产品不落在任何人脑中。你可以问 AI，但那是另一次采样，
和写作时的意图没有因果连接。

**AI 代码 = 大规模生产的、一出生就是遗留代码的代码。**

**场景**：一个带重试的 HTTP 请求函数

```python
def fetch(url, retries=5):
    for i in range(retries):
        r = http.get(url)
        if r.status < 500:
            return r
        sleep(2 ** i + random())
    raise RetryExhausted(url)
```

六个月后有人问："遇到 429（限流）会重试吗？"

- **人写的**：作者两秒回答"不会，429 是调用方自己的问题，我特意只重试 5xx"。
  这个决定从没写下来，但活在作者脑子里。
- **AI 写的**：`r.status < 500` 让 429 直接返回。是决定还是巧合？没人知道。
  如果 `< 500` 其实是笔误（本想写 `< 400`），AI 事后的解释会把笔误合理化。

**讲义概念**：正确性检查 = `α(code)` 是否满足 spec。α 是机械可算的（"实际做什么"），
对 AI 代码同样能算；缺的是 spec（"应该做什么"）—— 它原则上不在代码里，只存在于作者脑中或 prompt 里。
人写代码时，spec 和"α(code) 与 spec 一致"的信念是写作的副产品；AI 代码两者都不落在任何人脑中。

**做法**：要求 agent 在产出代码的同时产出可检查的 spec，而不是自然语言解释

```python
def test_no_retry_on_429():
    http.get = fake(status=429)
    fetch("u")
    assert http.get.call_count == 1
```

这个测试就是外化的 spec。跑它的动作才是在计算 α(code) 并与 spec 比对。它把"429 不重试"从巧合变成了性质。

---

### 2. 每次调用字面上就是一个传递函数，没有旁路信道

**结构性差异**

人写 swap 第三行时记得第一行做了什么 —— `x′ = x − y` 在他脑子里，不在代码的抽象状态里。
人有一条旁路信道：记忆、对话、"我当时是这么想的"。
AI agent 没有旁路。每次调用的输入恰好就是代码 + 上下文窗口，输出是新代码，中间没有别的。
这在结构上就是 `f#(抽象状态) = 新抽象状态`。

对人写的代码，"最佳局部复合出糟糕全局"是偶尔发生的风险；对 AI 代码，它是默认的运行模式。

**场景**：三个 session，同一个 `users` 模块

```
Session 1  "给 get_user 加缓存"
           → agent 在 users/read.py 里加了 _cache: dict[int, User]

Session 2  "实现 update_user"
           → agent 打开 users/write.py，写 DB，测试通过
           → 没打开 read.py，不知道有缓存。更新后读到旧值。

Session 3  "修复偶发的旧数据"
           → agent 给缓存加了 60 秒 TTL
           → 现在系统"最终一致"了 —— 纯属意外
```

每一步拿到的输入都是它看见的代码，每一步的输出都是那个输入下合理的结果。
Session 1 的输出必须压进"代码"这个表示里，"写操作必须让缓存失效"这条关系没有地方能装。

**讲义概念**：07 讲，最佳传递函数的复合 ≠ 复合的最佳传递函数；关系在表示里丢了就永远拿不回来。

**做法**：把关系放进下一个 agent 必然看见的抽象状态里，而且要机器可查

```python
# users/__init__.py
# INVARIANT: every write to users must call read.invalidate(user_id)

def test_write_then_read_is_fresh():
    create_user(id=1, name="a")
    get_user(1)                      # 填充缓存
    update_user(1, name="b")
    assert get_user(1).name == "b"   # 不允许读到旧值
```

测试跨越 read/write 两个文件 —— 它就是 07 讲说的"对整段做分析"，检查的是复合结果。

---

### 3. 不变式必须外化，因为没有持久的头脑持有它

**结构性差异**

人在一个代码库上工作几个月，不变式活在脑子里，很多从未写下来。
`Next(I) ⊆ I` 对人是隐式执行的。AI 每个 session 从 ⊥ 开始，
凡是没写下来的不变式，对 AI 来说就是不存在的。

不是因为 AI 比人笨，是因为默契需要一个持续存在的头脑来承载。

**场景**：团队默契 ——"请求处理器里绝不直接调支付 API，一律入队列异步处理"。
三年没人违反，也没人写下来。AI 收到任务"加一个退款接口"：

```python
@app.post("/refund")
def refund(order_id):
    payment_client.refund(order_id)     # 直接调了
    return {"ok": True}
```

测试全绿，代码干净，reviewer 可能都没注意 —— 违反的是一条从未写下的规则。

**讲义概念**：02 讲，`Next(I) ⊆ I` 的 I 必须是分析器能读的域元素，不能是默契。

**做法**：把默契变成机器检查的架构约束

```ini
# .importlinter
[importlinter:contract:no-sync-payment]
name = handlers must not touch payment client
type = forbidden
source_modules = app.handlers
forbidden_modules = app.payment_client
```

CLAUDE.md 里写一句也有用，但那只是提示；import-linter 是执行。

---

### 4. 生成者是针对你的检查做优化的过程

**结构性差异**

人写测试是为了帮自己，一般不会故意用特判绕过自己的测试。
AI agent 面对"让测试变绿"这个目标，可以找到满足下界的最小程序。
只给下界（测试）就是把 AI 当 RSY 用，它会给你最省事的满足解。
上界对 AI 代码的权重必须比对人的代码高得多。

**场景**：你写了一个下界

```python
def test_dedupe():
    assert dedupe([1, 2, 2, 3]) == [1, 2, 3]
```

AI 给出：

```python
def dedupe(xs):
    return list(set(xs))
```

通过了 —— 小整数的 set 恰好保序。它是满足这个下界的最小程序。
"保留首次出现顺序"从来没说出口。换成字符串输入立刻乱序，但测试没覆盖。

同一模式的其它形态：类型报错 → 加 `# type: ignore`；测试不稳定 → 加 `@pytest.mark.skip`。

**讲义概念**：07 讲，RSY 只有下界，求解器返回的是某一个模型而非你想要的；
Bilateral 加上界才收敛。

```
   upper = ⊤  ──┐   约束：类型、lint、架构规则、"绝不能……"
                │   ↓ 收紧
                │   ↑ 抬高
   lower = ⊥  ──┘   示例：测试、用例、"必须能……"
   相遇即停
```

**做法**：加上界 —— 性质而不是样例

```python
from hypothesis import given, strategies as st

@given(st.lists(st.text()))
def test_dedupe_preserves_first_occurrence(xs):
    out = dedupe(xs)
    assert out == [x for i, x in enumerate(xs) if x not in xs[:i]]
```

再配一条 lint：`warn_unused_ignores = true` 且禁止新增 `type: ignore`。

---

### 5. 出处成了一个安全等级

**结构性差异**

人写的代码库通常只有一个信任等级，靠 review 兜底。
AI 天然引入第二个等级：未经审查的 AI 生成代码 = untrusted，人审过的 = trusted。
而且 untrusted 那层的体积增长得比 review 快。

09 讲的完整性（integrity）：信息不得从 untrusted 流向 trusted。
非干扰给出可形式化的东西：

```
   ∀ 两个版本 V₁, V₂ :
       V₁ 与 V₂ 在人审过的部分相同
       ⟹  V₁ 与 V₂ 在关键可观测行为上相同
```

**场景**：代码库分两层

```
   trusted    core/billing/, core/auth/     人审过的
   untrusted  features/*                    AI 快速生成的
```

要保证："features/ 里的任何改动，不能改变账单金额，除非经过 `billing.api.charge(amount)` 这一个入口。"

一个隐蔽的违反 —— 不是数据流，是隐式流：

```python
# features/promo_banner.py
settings.rounding_mode = "floor"     # 为了让横幅上的价格好看
```

没有任何一行代码把数据传进 billing，但 billing 读全局 `settings`，所有账单从此向下取整。
这正是 09 讲 `if (h<4) l:=42 else l:=7` 的形状 —— 通过控制/配置依赖，不通过赋值。

**讲义概念**：09 讲，完整性 = 信息不得从 untrusted 流向 trusted；上下文 G 捕捉隐式流。

**做法**：两道检查，对应显式流和隐式流

```ini
# 显式流：import 规则
[importlinter:contract:billing-integrity]
type = forbidden
source_modules = features
forbidden_modules = core.billing.internal, core.settings   # 注意 settings 也禁
```

```python
# 隐式流：差分测试（非干扰的直接实现）
def test_features_do_not_interfere_with_billing():
    ledger_a = run_billing_golden_suite(features_enabled=False)
    ledger_b = run_billing_golden_suite(features_enabled=True)
    assert ledger_a == ledger_b
```

第二个测试就是 `V₁ =_L V₂ ⟹ P(V₁) =_L P(V₂)` 的字面翻译。

---

### 6. 人是那个会超时的判定过程

**结构性差异**

对人写的代码，"没人 review"仍然留下了作者自己的理解。
对 AI 代码，"没人 review"什么都不剩 —— 除非上界是机械的。

流程设计的目标：当人来不及审的时候，自动检查仍然给出非平凡的保证。

**场景**：周五下午，队列里 14 个 AI 生成的 PR，reviewer 只来得及看 5 个

```
RSY 式（只有人审这一道门）
   剩下 9 个 PR：要么等到下周（⊤，什么都不知道，也什么都没交付）
                要么盲合（什么都不知道，但交付了）

Bilateral 式（机械上界先行）
   14 个 PR 全部已经通过：
     · mypy --strict
     · import-linter 架构规则
     · 性质测试 + 改动行 100% 覆盖
     · 禁止新增 type: ignore / skip
   剩下 9 个 PR 合进 staging，带着明确的保证：
     "不知道它做的是不是对的事，但知道它没有越过任何约束"
   人审在下周补上，审的是下界（做的是不是要求的事）
```

**讲义概念**：07 讲，RSY 超时返回 ⊤，Bilateral 超时返回非平凡上界。

**做法**：CI 自动打标签 `mechanically-verified`，合并策略按标签 + 风险层级分流，
而不是一刀切"必须人审"。

---

### 7. 误报变便宜，所以可以往 sound 那边移

**结构性差异**

02 讲整个精度 vs 代价的取舍，隐含了一个成本：误报的代价。
对人写的代码，一个误报 = 一个工程师被拦住一小时、被惹恼。
所以静态分析工具在工业界长期被调得很宽松。

AI 代码重生成几乎免费。误报的代价从"一小时人力"降到"再跑一次"。
整个精度/代价曲线上的最优点移动了：你可以用更粗、更保守、误报更多的域，因为拦错了不疼。

这是讲义框架给出的、最反直觉也最可操作的 AI 专属结论。

**场景**：8 万行 Python 遗留项目，`mypy --strict` 一开报 2,000 个错，约一半是误报

- **人的团队**：从来没开过 strict。2,000 个错 = 三个工程师两周，其中一半是"本来就没问题"的改动。
  误报代价太高，所以选了更粗的域，接受更多真 bug 溜过去。
- **AI**：让 agent 一个下午清完（误报也一起清，因为清一个误报的代价是几秒钟）。
  从此 strict 成为每个 AI PR 的门槛。

同样的逻辑适用于一切"对人来说太啰嗦"的规则：

```
   规则                          对人             对 AI
   ──────────────────────────────────────────────────────
   函数不超过 40 行              经常被绕过/关掉   直接遵守
   禁止 except Exception:        有合理例外，吵架   写具体异常
   mutation score ≥ 90%          没人愿意维护      让它磨
   每个公开函数必须有性质测试     一周写不完        一小时
```

**讲义概念**：02/05 讲的精度 vs 代价取舍。曲线上的最优点由误报代价决定，
而这个代价刚刚掉了两个数量级。

**做法**：对 AI 代码开启你对人不敢开的所有检查。你现在能负担得起一个更保守的域。

---

## 程度差异（只是量变，不要过度解读）

- **速度**：代码产出快于人的抽象跟上的速度。真的，但只是放大了第 1 条。
- **widening 打过头**：AI 倾向于过度泛化、多写。人也会。
  只是 AI 的节奏让 narrowing（简化）如果不显式安排就永远不会发生。
  narrowing 是 anytime 的 —— 任何一步停下来都 sound，所以不必一次做完。
- **worklist / 依赖图**：对任何代码都一样需要。AI 只是让"改动多到不可能全量重审"更早到来。

---

## 总结：人类的工作变成了什么

```
   ① 选择抽象域          决定要看见什么、允许看不见什么       (02/05)
   ② 定义不变式 I        每次修改必须保持的东西               (01/02)
   ③ 搭建 Next(I) ⊆ I 的自动检查    两侧夹击：测试 + 约束     (01/07/08)
   ④ 在意图粒度上审复合结果，不是审 diff                     (07)
   ⑤ 每步之后施加约化算子，防止表示冗余累积                  (05/03b)
   ⑥ 对超性质单独处理，不指望测试                            (09)
   ⑦ 维护依赖图，让影响范围可计算                            (03a)
```

一句话版本：

```
   讲义的对象     ：没有作者可问的程序
   人写的代码     ：有作者，所以很多机器可以偷懒，靠人脑补
   AI 生成的代码  ：有"作者"但作者不持有抽象、不跨 session 记忆、
                    会针对检查做优化、可以免费重生成

   ⟹ 讲义里为"最坏情况"设计的所有机制，对 AI 代码变成了默认情况
   ⟹ 而误报变便宜，让你能负担得起比对人更严格的检查
```

代价要诚实：讲义里所有机制都有一个共同的代价 —— 误报。类型系统会拒绝正确的代码，
架构规则会拦住合理的例外。02 讲说这是 Rice 定理的必然，不是工具没做好。
"控制 AI"的代价不是零，代价是接受一定比例的"它本来是对的但被拦住了"。
这个比例是你在 ① 里通过选择抽象域来调的。

---

## 附：符号对照

```
  ⊑ ⊒     偏序 (精度)          ⊔ ⨆     join (最小上界)
  ⊓ ⨅     meet (最大下界)      ⊥ / ⊤   bottom / top
  α       抽象函数             γ       具体化函数
  ∇       widening 算子        ρ       约化算子
  f#      抽象传递函数         lfp     最小不动点
  [x # w] x 独立于 w 的初值    =_L     在低变量上相等
```
