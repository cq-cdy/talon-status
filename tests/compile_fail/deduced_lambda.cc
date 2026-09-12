// EXPECT: inconsistent types|must match previous return type|deduced as|C3487
#include "talon/status_macros.h"
int main() {
  auto function = [] {
    TALON_RETURN_IF_ERROR(talon::OkStatus());
    return talon::OkStatus();
  };
  (void)function();
}
