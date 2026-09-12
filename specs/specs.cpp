// specs/specs.cpp — 外化的 spec（"应该做什么"）。app/ 里只有 α（"实际做什么"）。
//
//   ./specs lower   示例：证明"至少能做到这些"   → 07 讲 Bilateral 的 lower；01 讲的动态分析
//   ./specs upper   性质：证明"绝不会做那些"     → 07 讲 Bilateral 的 upper
//   （HYPER 超性质由 gate 比对 golden_core / golden_full 两个二进制，09 讲）
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "app/core/billing.hpp"
#include "app/core/users.hpp"
#include "app/features/dedupe.hpp"

// ── 极简框架 ────────────────────────────────────────────────────────────────
namespace {
struct Failure {
  std::string msg;
};
struct Case {
  const char* tier;
  const char* name;
  void (*fn)();
};
std::vector<Case>& cases() {
  static std::vector<Case> all;
  return all;
}
struct Reg {
  Reg(const char* t, const char* n, void (*f)()) { cases().push_back({t, n, f}); }
};
#define SPEC(tier, name)                              \
  void name();                                        \
  const Reg name##_reg(#tier, #name, name);           \
  void name()
#define CHECK(cond, msg) \
  if (!(cond)) throw Failure { msg }

// 每个 spec 从 ⊥ 开始 —— 状态不跨用例泄漏
void fresh() {
  app::core::reset_billing();
  app::core::reset_users();
  app::core::settings().rounding_mode = app::core::Rounding::HalfEven;
}
}  // namespace

// ── LOWER：示例 ───────────────────────────────────────────────────────────────

// 跨文件不变式（users.hpp 顶部）。检查的是 write→read 的复合结果，不是单步（07 讲）。
SPEC(lower, write_then_read_is_fresh) {
  fresh();
  app::core::users_db()[1] = {1, "a"};
  CHECK(app::core::get_user(1).name == "a", "初次读取失败");  // 填充缓存
  app::core::update_user(1, "b");
  CHECK(app::core::get_user(1).name == "b", "update_user 之后 get_user 读到旧值 'a'：写操作没有让缓存失效");
}

// 第 4 条：只有这个示例时，agent 会给出满足它的最小程序（std::set —— 小整数恰好有序）。
SPEC(lower, dedupe_example) {
  const std::vector<int> want{1, 2, 3};
  CHECK(app::features::dedupe(std::vector<int>{1, 2, 2, 3}) == want, "dedupe({1,2,2,3}) != {1,2,3}");
}

// ── UPPER：性质 ───────────────────────────────────────────────────────────────

namespace {
using Strs = std::vector<std::string>;
Strs reference(const Strs& xs) {  // 保留首次出现顺序
  Strs out;
  for (const auto& x : xs) {
    bool seen = false;
    for (const auto& y : out) seen = seen || y == x;
    if (!seen) out.push_back(x);
  }
  return out;
}
bool holds(const Strs& xs) { return app::features::dedupe(xs) == reference(xs); }
std::string show(const Strs& xs) {
  std::string s = "[";
  for (std::size_t i = 0; i < xs.size(); ++i) s += (i ? ", \"" : "\"") + xs[i] + "\"";
  return s + "]";
}
Strs shrink(Strs xs) {  // 朴素收缩：逐个去掉元素，仍失败就保留更短的
  for (bool changed = true; changed && xs.size() > 1;) {
    changed = false;
    for (std::size_t i = 0; i < xs.size() && !changed; ++i) {
      Strs c = xs;
      c.erase(c.begin() + static_cast<std::ptrdiff_t>(i));
      if (!holds(c)) xs = c, changed = true;
    }
  }
  return xs;
}
}  // namespace

// 对随机输入成立的性质。失败时打印的反例 = 07 讲 RSY 里求解器返回的 S。
SPEC(upper, dedupe_preserves_first_occurrence) {
  std::mt19937 rng(0);
  std::uniform_int_distribution<int> len(0, 8), slen(0, 3), ch(0, 1);
  for (int t = 1; t <= 300; ++t) {
    Strs xs;
    for (int i = len(rng); i > 0; --i) {
      std::string s;
      for (int j = slen(rng); j > 0; --j) s += ch(rng) ? 'b' : 'a';
      xs.push_back(s);
    }
    if (!holds(xs)) {
      const Strs small = shrink(xs);
      throw Failure{"性质不成立。第 " + std::to_string(t) + " 次试验找到反例，收缩后: " + show(small) +
                    "  →  dedupe 给出 " + show(app::features::dedupe(small)) + "，应为 " + show(reference(small))};
    }
  }
}

// ── 入口 ──────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
  const std::string tier = argc > 1 ? argv[1] : "";
  int passed = 0, failed = 0;
  for (const auto& c : cases()) {
    if (!tier.empty() && tier != c.tier) continue;
    try {
      c.fn();
      ++passed;
      std::printf("  ok   %s\n", c.name);
    } catch (const Failure& f) {
      ++failed;
      std::printf("  FAIL %s\n       %s\n", c.name, f.msg.c_str());
    }
  }
  std::printf("%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
