#include "app/core/billing.hpp"

namespace app::core {

Settings& settings() {
  static Settings s;
  return s;
}

namespace {
std::vector<Cents>& book() {
  static std::vector<Cents> b;
  return b;
}
}  // namespace

Cents charge(Milli amount) {
  const Milli q = amount / 10;
  const Milli r = amount % 10;
  Cents c = q;
  if (settings().rounding_mode == Rounding::HalfEven) c = r > 5 ? q + 1 : r < 5 ? q : (q % 2 == 0 ? q : q + 1);
  book().push_back(c);
  return c;
}

std::vector<Cents> ledger() { return book(); }
void reset_billing() { book().clear(); }

}  // namespace app::core
