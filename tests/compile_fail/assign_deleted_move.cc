// EXPECT: deleted|C2280
#include "talon/status_macros.h"

struct CopyOnly {
  CopyOnly() = default;
  CopyOnly(const CopyOnly&) = default;
  CopyOnly(CopyOnly&&) = delete;
};

talon::Status Run() {
  TALON_ASSIGN_OR_RETURN(CopyOnly value,
                         talon::StatusOr<CopyOnly>(talon::in_place));
  (void)value;
  return talon::OkStatus();
}
int main() {}
