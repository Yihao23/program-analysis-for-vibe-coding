#pragma once
// agent 的产出：满足 dedupe_example 的最短程序。"保留首次出现顺序"从没人说出口，它没有义务猜。
#include <set>
#include <vector>

namespace app::features {

template <typename T>
std::vector<T> dedupe(const std::vector<T>& xs) {
  std::set<T> s(xs.begin(), xs.end());
  return std::vector<T>(s.begin(), s.end());
}

}  // namespace app::features
