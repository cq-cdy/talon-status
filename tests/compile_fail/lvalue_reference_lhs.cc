// EXPECT: cannot bind non-const lvalue reference|non-const lvalue reference.*cannot bind|C2440
#include "talon/status_macros.h"
talon::Status Work() {
  TALON_ASSIGN_OR_RETURN(int& value, talon::StatusOr<int>(1));
  (void)value;
  return talon::OkStatus();
}
int main() {}
