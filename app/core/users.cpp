#include "app/core/users.hpp"

namespace app::core {

std::map<int, User>& users_db() {
  static std::map<int, User> table;
  return table;
}

namespace {
std::map<int, User>& cache() {
  static std::map<int, User> hot;
  return hot;
}
}  // namespace

User get_user(int id) {
  if (const auto it = cache().find(id); it != cache().end()) return it->second;
  const User u = users_db().at(id);
  cache()[id] = u;
  return u;
}

void invalidate(int id) { cache().erase(id); }

void reset_users() {
  users_db().clear();
  cache().clear();
}

}  // namespace app::core
