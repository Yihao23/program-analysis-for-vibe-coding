// 非干扰（09 讲）的观测器：对固定输入打印账本。不判断对错 —— 判断由 gate 比对两个二进制完成。
//   golden_core  只链接 core          golden_full  链接 core + features
//   ∀ V₁, V₂ :  V₁ =_trusted V₂  ⟹  P(V₁) =_ledger P(V₂)
#include <cstdio>

#include "app/core/billing.hpp"

int main() {
  for (const auto a : {10005, 2675, 125, 99995}) app::core::charge(a);
  const char* sep = "";
  for (const auto c : app::core::ledger()) {
    std::printf("%s%lld.%02lld", sep, static_cast<long long>(c / 100), static_cast<long long>(c % 100));
    sep = " ";
  }
  std::printf("\n");
}
