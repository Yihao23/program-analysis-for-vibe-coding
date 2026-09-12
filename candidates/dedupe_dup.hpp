#pragma once
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

// agent 在另一个 session 里加的：它没看见上面那个函数，或者觉得名字不对。
// 同一个具体行为的第二个表示 —— 这就是腐烂的第一步。
template <typename T>
std::vector<T> unique(const std::vector<T>& items) {
  std::unordered_set<T> seen;
  std::vector<T> out;
  for (const auto& x : items)
    if (seen.insert(x).second) out.push_back(x);
  return out;
}

}  // namespace app::features
