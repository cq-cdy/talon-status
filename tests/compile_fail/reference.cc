// EXPECT: StatusOr requires
#include "talon/status_or.h"
talon::StatusOr<int&> value;
int main() {}
