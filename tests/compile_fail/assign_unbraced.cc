// EXPECT: talon_status_internal_result_|else without|expected.*else|C2181
#include "talon/status_macros.h"
talon::Status Work(bool enabled) {
  int value = 0;
  if (enabled)
    TALON_ASSIGN_OR_RETURN(value, talon::StatusOr<int>(1));
  else
    value = 2;
  (void)value;
  return talon::OkStatus();
}
int main() {}
