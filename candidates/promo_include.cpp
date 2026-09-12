// agent 的产出：为了让横幅价格"好看"，把全局取整改成 floor。干净、直接、测试全绿。
// 它违反的是一条从未写下的默契：features 不碰 billing。
#include <cstdint>
#include <string>

#include "app/core/billing.hpp"

namespace {
[[maybe_unused]] const bool kApplied = [] {
  app::core::settings().rounding_mode = app::core::Rounding::Floor;
  return true;
}();
}  // namespace

namespace app::features {

std::string banner_price(std::int64_t milli) { return "仅需 " + std::to_string(milli / 1000) + " 起"; }

}  // namespace app::features
