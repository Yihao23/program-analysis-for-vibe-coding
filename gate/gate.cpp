// gate/gate.cpp — 门禁：Next(I) ⊆ I 的检查器
//
//   gate check [FILE ...]   跑全部检查，按 UPPER / LOWER / HYPER 报告；FILE 是这次改动的文件
//   gate affected FILE ...  worklist：改了这些文件，要重查什么（03a 讲 Dep_b）
//   gate demo [N]           把 candidates/ 里的 agent 产出依次装进来跑门禁
//
// 报告结构来自 07 讲 Bilateral：
//   UPPER  约束 —— 绝不能。机械、可无人值守。人来不及审时这是剩下的全部保证。
//   LOWER  示例 —— 必须能。证明做的是要求的事。
//   HYPER  超性质 —— 两次运行比对（09 讲）。
#include <sys/wait.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
static const fs::path ROOT = GATE_ROOT;
static const fs::path BUILD = GATE_BUILD;

// ═══ 抽象域声明 —— "我们决定看见什么"（02/05 讲）。改这一段 = 改精度/代价的取舍。这是人保留的工作。═══

// 09 讲：完整性格。untrusted 可以快速增长；trusted 的改动必须经人审。
static const std::vector<std::string> TRUSTED = {"app/core"};
static const std::vector<std::string> UNTRUSTED = {"app/features"};

// 02/03 讲：外化的不变式 I（显式流 / #include 契约）。第 3 条：曾经是默契，现在是机器读的东西。
struct Contract {
  std::string name, source;
  std::vector<std::string> forbidden;
};
static const std::vector<Contract> CONTRACTS = {
    {"features 不得触碰 billing（含全局设置）", "app/features", {"app/core/billing"}},
};

// 03b 讲：阈值集 K。widening 跳到这些常数，不逐次协商。
static const std::size_t K_MAX_FUNCTION_LINES = 40, K_MAX_INCLUDES = 6;

// 07 讲：生成者会针对检查做优化。第 4 条：绕过检查的逃生口，对 AI 一律禁止新增。
static const std::vector<std::string> ESCAPE_HATCHES = {"NOLINT", "#pragma GCC diagnostic", "#pragma clang diagnostic",
                                                        "-Wno-", "const_cast", "reinterpret_cast"};

// 07 讲：误报变便宜 → clang-tidy 警告即错误
static const std::string TIDY_ARGS = "--checks='-*,bugprone-*,performance-*' --warnings-as-errors='*'";

// ═══ 工具 ═══════════════════════════════════════════════════════════════════════

struct Cmd {
  int code;
  std::string out;
};
static Cmd run(const std::string& cmd) {
  FILE* p = popen((cmd + " 2>&1").c_str(), "r");
  if (p == nullptr) return {127, ""};
  std::string out;
  char buf[4096];
  while (fgets(buf, sizeof buf, p) != nullptr) out += buf;
  const int st = pclose(p);
  return {WIFEXITED(st) ? WEXITSTATUS(st) : -1, out};
}
static std::string read_file(const fs::path& p) {
  std::stringstream ss;
  ss << std::ifstream(p).rdbuf();
  return ss.str();
}
static void write_file(const fs::path& p, const std::string& s) { std::ofstream(p, std::ios::trunc) << s; }
static std::string rel(const fs::path& p) { return fs::relative(p, ROOT).generic_string(); }
static bool starts_with(const std::string& s, const std::string& pre) { return s.rfind(pre, 0) == 0; }
static std::string trim(const std::string& s) {
  const auto b = s.find_first_not_of(" \t\r\n"), e = s.find_last_not_of(" \t\r\n");
  return b == std::string::npos ? "" : s.substr(b, e - b + 1);
}
static std::vector<std::string> lines_of(const std::string& s) {
  std::vector<std::string> v;
  std::stringstream ss(s);
  for (std::string l; std::getline(ss, l);) v.push_back(l);
  return v;
}
static std::vector<fs::path> sources_under(const std::string& dir) {
  std::vector<fs::path> v;
  for (const auto& e : fs::recursive_directory_iterator(ROOT / dir))
    if (e.is_regular_file() && (e.path().extension() == ".cpp" || e.path().extension() == ".hpp")) v.push_back(e.path());
  std::sort(v.begin(), v.end());
  return v;
}
static std::string tier_of(const std::string& r) {
  for (const auto& t : TRUSTED) if (starts_with(r, t)) return "trusted";
  for (const auto& u : UNTRUSTED) if (starts_with(r, u)) return "untrusted";
  return "other";
}
static std::vector<std::string> includes_of(const std::string& src) {
  static const std::regex inc(R"re(^\s*#\s*include\s*"([^"]+)")re");
  std::vector<std::string> v;
  for (const auto& l : lines_of(src))
    if (std::smatch m; std::regex_search(l, m, inc)) v.push_back(m[1]);
  return v;
}
// 去掉注释，保留换行（行号不变）
static std::string strip_comments(const std::string& s) {
  std::string o;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/') {
      while (i < s.size() && s[i] != '\n') ++i;
      o += '\n';
    } else if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*') {
      for (i += 2; i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/'); ++i)
        if (s[i] == '\n') o += '\n';
      ++i;
    } else {
      o += s[i];
    }
  }
  return o;
}
// 极简函数提取：name(params) [const] [noexcept] {  + 花括号匹配。不是 C++ 解析器；够 K 和 ρ 用。
struct Func {
  std::string name, params, body;
  std::size_t line, lines;
};
static std::vector<Func> functions_of(const std::string& raw) {
  static const std::regex head(R"re(([A-Za-z_][A-Za-z0-9_:~]*)\s*\(([^()]*)\)\s*(?:const\s*)?(?:noexcept\s*)?\{)re");
  static const std::set<std::string> kw = {"if", "for", "while", "switch", "catch", "return", "do"};
  const std::string src = strip_comments(raw);
  std::vector<Func> v;
  for (std::size_t pos = 0; pos < src.size();) {
    std::smatch m;
    if (!std::regex_search(src.cbegin() + static_cast<std::ptrdiff_t>(pos), src.cend(), m, head)) break;
    const std::size_t start = pos + static_cast<std::size_t>(m.position(0));
    const std::size_t brace = start + static_cast<std::size_t>(m.length(0)) - 1;
    const std::string name = m[1];
    if (kw.count(name.substr(name.rfind(':') + 1)) != 0) { pos = brace + 1; continue; }
    std::size_t end = brace;
    for (int depth = 0; end < src.size(); ++end) {
      if (src[end] == '{') ++depth;
      if (src[end] == '}' && --depth == 0) break;
    }
    const auto nl = [&](std::size_t a, std::size_t b) {
      return static_cast<std::size_t>(std::count(src.begin() + static_cast<std::ptrdiff_t>(a), src.begin() + static_cast<std::ptrdiff_t>(b), '\n'));
    };
    v.push_back({name, m[2], src.substr(brace + 1, end - brace - 1), nl(0, start) + 1, nl(start, end) + 1});
    pos = end + 1;
  }
  return v;
}

// ═══ 各项检查 ═══════════════════════════════════════════════════════════════════

static const char *OK = "✓", *FAIL = "✗", *SKIP = "○";
struct Result {
  std::string name, status, tag;
  std::vector<std::string> detail;
};
static Result of(const std::string& name, std::vector<std::string> v, const std::string& tag) {
  return {name, v.empty() ? OK : FAIL, tag, std::move(v)};
}

// 显式流：#include 契约。对 extern 前置声明 / dlopen 是盲的 —— 这个盲区正是 HYPER 存在的理由。
static std::vector<std::string> check_arch(const std::vector<fs::path>& files) {
  std::vector<std::string> v;
  for (const auto& p : files)
    for (const auto& c : CONTRACTS)
      if (starts_with(rel(p), c.source))
        for (const auto& inc : includes_of(read_file(p)))
          for (const auto& fb : c.forbidden)
            if (starts_with(inc, fb)) v.push_back(rel(p) + ": #include \"" + inc + "\"  违反「" + c.name + "」");
  return v;
}
static std::vector<std::string> check_escape(const std::vector<fs::path>& files) {
  std::vector<std::string> v;
  for (const auto& p : files) {
    const auto ls = lines_of(read_file(p));
    for (std::size_t i = 0; i < ls.size(); ++i)
      for (const auto& h : ESCAPE_HATCHES)
        if (ls[i].find(h) != std::string::npos) v.push_back(rel(p) + ":" + std::to_string(i + 1) + " 含逃生口 `" + h + "`");
  }
  return v;
}
static std::vector<std::string> check_K(const std::vector<fs::path>& files) {
  std::vector<std::string> v;
  for (const auto& p : files) {
    const std::string src = read_file(p);
    if (const auto n = includes_of(src).size(); n > K_MAX_INCLUDES)
      v.push_back(rel(p) + ": " + std::to_string(n) + " 个 #include > K=" + std::to_string(K_MAX_INCLUDES));
    for (const auto& f : functions_of(src))
      if (f.lines > K_MAX_FUNCTION_LINES)
        v.push_back(rel(p) + ":" + std::to_string(f.line) + " " + f.name + "() 有 " + std::to_string(f.lines) + " 行 > K=" +
                    std::to_string(K_MAX_FUNCTION_LINES));
  }
  return v;
}
// ρ 的简化版（05 讲 reduced product）：函数体归一化后相同 ⟹ 同一具体行为的冗余表示。
static std::string fingerprint(const Func& f) {
  static const std::regex tail(R"re(([A-Za-z_][A-Za-z0-9_]*)\s*$)re");
  std::string body = f.body;
  std::stringstream ps(f.params);
  std::size_t n = 0;
  for (std::string param; std::getline(ps, param, ',');) {
    param = param.substr(0, param.find('='));
    if (std::smatch m; std::regex_search(param, m, tail))
      body = std::regex_replace(body, std::regex("\\b" + std::string(m[1]) + "\\b"), "_arg" + std::to_string(n++));
  }
  std::string o;
  for (const char c : body)
    if (std::isspace(static_cast<unsigned char>(c)) == 0) o += c;
  return o;
}
static std::vector<std::string> check_reduce(const std::vector<fs::path>& files) {
  std::map<std::string, std::vector<std::string>> seen;
  for (const auto& p : files)
    for (const auto& f : functions_of(read_file(p)))
      if (std::count(f.body.begin(), f.body.end(), ';') >= 2)  // 一行函数不算
        seen[fingerprint(f)].push_back(rel(p) + ":" + std::to_string(f.line) + " " + f.name + "()");
  std::vector<std::string> v;
  for (const auto& [fp, locs] : seen)
    if (locs.size() > 1) {
      std::string s = "冗余表示（函数体相同）: ";
      for (std::size_t i = 0; i < locs.size(); ++i) s += (i ? "  ≡  " : "") + locs[i];
      v.push_back(s);
    }
  return v;
}
static std::vector<std::string> grep(const std::string& out, const std::vector<std::string>& keys, std::size_t max = 6) {
  std::vector<std::string> v;
  for (const auto& l : lines_of(out))
    for (const auto& k : keys)
      if (l.find(k) != std::string::npos && v.size() < max) { v.push_back(trim(l)); break; }
  return v;
}
static Result build() {
  const Cmd c = run("cmake --build '" + BUILD.string() + "'");
  return {"build", c.code == 0 ? OK : FAIL, "07 -Werror：编译器本身是一道上界", c.code == 0 ? std::vector<std::string>{} : grep(c.out, {"error"})};
}
static Result tidy() {
  if (run("command -v clang-tidy").code != 0) return {"clang-tidy", SKIP, "07 误报变便宜 → 警告即错误", {"未安装"}};
  std::string files;
  for (const auto& p : sources_under("app")) if (p.extension() == ".cpp") files += " '" + p.string() + "'";
  const Cmd c = run("clang-tidy -p '" + BUILD.string() + "' --quiet " + TIDY_ARGS + files);
  auto d = grep(c.out, {"warning:", "error:"});
  for (auto& l : d) if (starts_with(l, ROOT.string())) l = l.substr(ROOT.string().size() + 1);
  return {"clang-tidy", c.code == 0 ? OK : FAIL, "07 误报变便宜 → 警告即错误", d};
}
static Result specs(const std::string& tier, const std::string& tag) {
  const Cmd c = run((BUILD / "specs").string() + " " + tier);
  const auto ls = lines_of(c.out);
  std::vector<std::string> d{ls.empty() ? "" : trim(ls.back())};
  if (c.code != 0) for (const auto& l : grep(c.out, {"FAIL", "反例", "旧值"})) d.push_back(l);
  return {tier, c.code == 0 ? OK : FAIL, tag, d};
}
static Result hyper() {
  const std::string a = trim(run((BUILD / "golden_core").string()).out), b = trim(run((BUILD / "golden_full").string()).out);
  if (a == b) return {"noninterf", OK, "09 非干扰 · 隐式流", {"两个世界的账本一致: " + a}};
  return {"noninterf", FAIL, "09 非干扰 · 隐式流", {"features/ 改变了 billing 的行为（隐式流）:", "  不链接 features: " + a, "  链接 features:   " + b}};
}

// ═══ 报告 ═══════════════════════════════════════════════════════════════════════

static bool any_fail(const std::vector<Result>& rs) {
  return std::any_of(rs.begin(), rs.end(), [](const Result& r) { return r.status == FAIL; });
}
static void section(const char* title, const std::vector<Result>& rs) {
  std::printf("%s\n", title);
  for (const auto& r : rs) {
    std::printf("  %s %-10s %s\n", r.status.c_str(), r.name.c_str(), r.tag.c_str());
    for (const auto& d : r.detail) if (!d.empty()) std::printf("        %s\n", d.c_str());
  }
}
static int check(const std::vector<fs::path>& changed) {
  const auto scope = changed.empty() ? sources_under("app") : changed;
  const Result b = build();
  const bool built = b.status == OK;
  const Result skipped{"", SKIP, "", {"构建失败，未运行"}};
  auto unless_built = [&](const char* name, const char* tag, auto f) {
    if (built) return f();
    Result s = skipped; s.name = name; s.tag = tag; return s;
  };
  const std::vector<Result> upper = {
      b,
      of("arch", check_arch(scope), "02/03 外化不变式 · 显式流"),
      of("escape", check_escape(scope), "07 生成者会针对检查优化"),
      of("K", check_K(scope), "03b 阈值集 K"),
      of("reduce", check_reduce(sources_under("app")), "05 约化积 ρ · 防腐烂"),
      tidy(),
      unless_built("upper", "07 Bilateral upper · 性质测试", [] { return specs("upper", "07 Bilateral upper · 性质测试"); }),
  };
  const std::vector<Result> lower = {unless_built("lower", "07 Bilateral lower · 示例测试", [] { return specs("lower", "07 Bilateral lower · 示例测试"); })};
  const std::vector<Result> hyp = {unless_built("noninterf", "09 非干扰 · 隐式流", [] { return hyper(); })};

  const std::string bar(72, '=');
  std::printf("%s\nGATE REPORT     Next(I) ⊆ I ?\n", bar.c_str());
  for (const auto& p : changed) std::printf("  改动: %s   [%s]\n", rel(p).c_str(), tier_of(rel(p)).c_str());
  std::printf("%s\n", std::string(72, '-').c_str());
  section("UPPER  约束 —— 绝不能", upper);
  section("LOWER  示例 —— 必须能", lower);
  section("HYPER  超性质 —— 两次运行比对", hyp);
  const bool u = !any_fail(upper) && !any_fail(hyp), l = !any_fail(lower);
  std::printf("%s\nVERDICT  %s\n%s\n", std::string(72, '-').c_str(),
              u && l ? "上界 ✓ 下界 ✓ → 可合并"
              : u    ? "上界 ✓ 下界 ✗ → 没越界，但没做到要求的事（等人审 / 再生成）"
              : l    ? "上界 ✗ 下界 ✓ → 做到了要求的事，但越界了（拒绝，不需要人审）"
                     : "上界 ✗ 下界 ✗ → 拒绝",
              bar.c_str());
  return u && l ? 0 : 1;
}

// ═══ worklist（03a 讲）：Dep_b = 谁 include 了 b；改了 b 只需重查 Dep_b 的闭包 ═══════

static int affected(const std::vector<fs::path>& changed) {
  std::map<std::string, std::set<std::string>> dep;
  auto files = sources_under("app");
  for (const auto& s : sources_under("specs")) files.push_back(s);
  for (const auto& f : files) for (const auto& inc : includes_of(read_file(f))) dep[inc].insert(rel(f));
  std::set<std::string> seen;
  std::vector<std::string> work;
  auto seed = [&](const std::string& r) { if (seen.insert(r).second) work.push_back(r); };
  for (const auto& c : changed) {
    seed(rel(c));
    if (const auto h = fs::path(rel(c)).replace_extension(".hpp").generic_string(); fs::exists(ROOT / h)) seed(h);
  }
  while (!work.empty()) {
    const std::string b = work.back();
    work.pop_back();
    for (const auto& d : dep[b]) seed(d);
  }
  std::printf("worklist 闭包（03a 讲 Dep_b）:\n");
  for (const auto& c : changed) std::printf("  改动: %s\n", rel(c).c_str());
  std::printf("  受影响的文件:\n");
  for (const auto& r : seen) std::printf("    %s\n", r.c_str());
  std::printf("  → %s\n", seen.count("specs/specs.cpp") ? "需要重跑 specs" : "没有 spec 覆盖这次改动 —— 这本身就是一个发现");
  return 0;
}

// ═══ demo：把 candidates/ 里的 agent 产出依次装进项目，跑门禁，展示每道检查抓住了什么 ═══

struct Scenario {
  const char *title, *story, *candidate, *target;
};
static const std::vector<Scenario> SCENARIOS = {
    {"基线：当前代码库", "所有检查都过 —— 这是 I。之后每个场景都是一次 Next。", nullptr, nullptr},
    {"dedupe_set —— 只有下界时 agent 给出的最小程序",
     "下界（示例）过；上界（性质）失败并返回反例。07 讲：RSY 只有下界，求解器返回的是某一个模型而非你想要的。",
     "dedupe_set.hpp", "app/features/dedupe.hpp"},
    {"dedupe_dup —— 功能全对，但多了一个等价函数",
     "所有测试过；ρ 抓到冗余表示。05 讲：同一具体值的多个表示，不约化则精度流失 —— 腐烂的机制。",
     "dedupe_dup.hpp", "app/features/dedupe.hpp"},
    {"promo_include —— 违反一条从未写下的默契",
     "测试全绿；#include 契约失败。第 3 条：没写下来的不变式对 AI 不存在，写成机器读的形式它就存在了。",
     "promo_include.cpp", "app/features/promo.cpp"},
    {"promo_extern —— 静态规则过了，非干扰抓住了",
     "#include 契约过（前置声明绕开了它）；HYPER 失败。09 讲：隐式流。单次运行看不出来，两个世界比对才看得出来。",
     "promo_extern.cpp", "app/features/promo.cpp"},
    {"users_write_noinv —— 另一个 session 的 agent 没看见缓存",
     "单文件看不出问题；跨文件不变式测试失败。第 2 条 / 07 讲：关系装不进单文件的抽象状态，检查必须跨文件。",
     "users_write_noinv.cpp", "app/core/users_write.cpp"},
};
static int demo(int only) {
  const std::string bar(72, '#');
  for (std::size_t i = 0; i < SCENARIOS.size(); ++i) {
    if (only >= 0 && static_cast<std::size_t>(only) != i) continue;
    const auto& s = SCENARIOS[i];
    std::printf("\n\n%s\n# 场景 %zu: %s\n# %s\n%s\n", bar.c_str(), i, s.title, s.story, bar.c_str());
    if (s.candidate == nullptr) { check({}); continue; }
    const fs::path dst = ROOT / s.target;
    const std::string backup = read_file(dst);
    write_file(dst, read_file(ROOT / "candidates" / s.candidate));
    check({dst});
    write_file(dst, backup);
  }
  if (only < 0) {
    std::printf("\n\n%s\n# 附：worklist —— 改了 users.hpp，需要重查什么？\n%s\n", bar.c_str(), bar.c_str());
    affected({ROOT / "app/core/users.hpp"});
  }
  run("cmake --build '" + BUILD.string() + "'");  // 还原后重建，让 build/ 与源码一致
  return 0;
}

int main(int argc, char** argv) {
  const std::string cmd = argc > 1 ? argv[1] : "";
  std::vector<fs::path> files;
  for (int i = 2; i < argc; ++i) files.push_back(fs::absolute(ROOT / argv[i]));
  if (cmd == "check") return check(files);
  if (cmd == "affected" && !files.empty()) return affected(files);
  if (cmd == "demo") return demo(argc > 2 ? std::atoi(argv[2]) : -1);
  std::printf("gate check [FILE ...]\ngate affected FILE ...\ngate demo [N]\n");
  return 2;
}
