#include "app/core/users.hpp"

namespace app::core {

User update_user(int id, const std::string& name) {
  const User u{id, name};
  users_db()[id] = u;
  invalidate(id);  // INVARIANT: 见 users.hpp 顶部
  return u;
}

}  // namespace app::core
