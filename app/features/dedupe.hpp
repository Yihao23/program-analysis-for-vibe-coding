#pragma once
// 这个文件里只有 α（"实际做什么"）。
// spec（"应该做什么"：保留首次出现的顺序）不在这里，在 specs/specs.cpp。
// 分开放是刻意的：代码永远无法告诉你它的行为是不是被想要的。
#include <unordered_set>
#include <vector>

namespace app::features {

template <typename T>
std::vector<T> dedupe(const std::vector<T>& xs) {
  std::unordered_set<T> seen;
  std::vector<T> out;
  for (const auto& x : xs)
    if (seen.insert(x).second) out.push_back(x);
  return out;
}

}  // namespace app::features
