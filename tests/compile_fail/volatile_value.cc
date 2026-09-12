// EXPECT: StatusOr requires
#include "talon/status_or.h"
talon::StatusOr<volatile int> value;
int main() {}
