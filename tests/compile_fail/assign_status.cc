// EXPECT: no member named.*value|has no member named.*value|value.*not a member|C2039
#include "talon/status_macros.h"
talon::Status Work() {
  TALON_ASSIGN_OR_RETURN(int value, talon::OkStatus());
  (void)value;
  return talon::OkStatus();
}
int main() {}
