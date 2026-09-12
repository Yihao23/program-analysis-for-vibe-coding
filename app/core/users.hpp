#pragma once
// INVARIANT（跨文件关系 —— 02 讲的 I；07 讲说它装不进单个文件的抽象状态）:
//     每一次写操作（users_write.cpp）之后，必须调用 invalidate(id) 清掉 users.cpp 里的缓存。
//     单看 users_write.cpp 看不出为什么，单看 users.cpp 看不出谁负责。它只能活在这里，
//     并由 specs/specs.cpp 的 write_then_read_is_fresh 机械地检查。
#include <map>
#include <string>

namespace app::core {

struct User {
  int id;
  std::string name;
};

std::map<int, User>& users_db();                    // users.cpp
User get_user(int id);                              // users.cpp，带缓存
void invalidate(int id);                            // users.cpp
User update_user(int id, const std::string& name);  // users_write.cpp
void reset_users();

}  // namespace app::core
