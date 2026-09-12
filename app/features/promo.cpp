// 横幅价格显示（untrusted）。向下取整只影响显示，不碰全局设置。
#include <cstdint>
#include <string>

namespace app::features {

std::string banner_price(std::int64_t milli) { return "仅需 " + std::to_string(milli / 1000) + " 起"; }

}  // namespace app::features
