#pragma once
// billing（trusted）。features/ 不得 #include 本文件 —— 见 gate/gate.cpp CONTRACTS。
//
// settings() 是"隐式流"的源头（09 讲的上下文 G）：谁改了它，所有读它的地方都变，
// 而这在 #include 图和数据流里都看不见。
//
// 非干扰性质（specs/golden.cpp + gate 的 HYPER 段）：
//   两个版本若在 trusted 部分相同，charge() 的账本必须相同，无论 features/ 做了什么。
#include <cstdint>
#include <vector>

namespace app::core {

enum class Rounding : std::uint8_t { HalfEven, Floor };
struct Settings {
  Rounding rounding_mode = Rounding::HalfEven;
};
Settings& settings();

using Milli = std::int64_t;  // 千分之一元：10.005 元 = 10005
using Cents = std::int64_t;  // 分
Cents charge(Milli amount);
std::vector<Cents> ledger();
void reset_billing();

}  // namespace app::core
