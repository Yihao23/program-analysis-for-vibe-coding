#include "app/core/users.hpp"

namespace app::core {

// agent 在 Session 2 里写的：它打开的是 users_write.cpp，没打开 users.cpp，不知道那里有缓存。
// 这一步在它看到的输入下是"最佳"的。
User update_user(int id, const std::string& name) {
  const User u{id, name};
  users_db()[id] = u;
  return u;
}

}  // namespace app::core
