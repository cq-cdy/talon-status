// EXPECT: could not convert|no viable conversion|cannot convert|C2440
#include "talon/status_or.h"
struct Explicit { explicit Explicit(int) {} };
talon::StatusOr<Explicit> Make() { return 1; }
int main() {}
