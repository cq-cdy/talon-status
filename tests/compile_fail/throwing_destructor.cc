// EXPECT: StatusOr requires
#include "talon/status_or.h"
struct ThrowingDestructor { ~ThrowingDestructor() noexcept(false) {} };
talon::StatusOr<ThrowingDestructor> value;
int main() {}
