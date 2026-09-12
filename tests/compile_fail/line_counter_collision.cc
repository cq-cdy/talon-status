// EXPECT: conflicting declaration|redefinition|C2374|C2086
#define TALON_STATUS_USE_LINE_COUNTER 1
#include "talon/status_macros.h"
talon::Status Work() {
  TALON_ASSIGN_OR_RETURN(int a, talon::StatusOr<int>(1)); TALON_ASSIGN_OR_RETURN(int b, talon::StatusOr<int>(2));
  (void)a;
  (void)b;
  return talon::OkStatus();
}
int main() {}
