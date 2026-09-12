// EXPECT: macro.*passed 3 arguments|too many arguments.*macro|too many.*macro|C4002
#include <utility>
#include "talon/status_macros.h"
talon::Status Work() {
  TALON_ASSIGN_OR_RETURN(std::pair<int, int> value, (talon::StatusOr<std::pair<int, int> >(talon::in_place, 1, 2)));
  return talon::OkStatus();
}
int main() {}
