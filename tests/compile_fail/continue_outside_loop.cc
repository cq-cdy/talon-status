// EXPECT: continue statement not within a loop|continue.*not in loop|continue.*only.*loop|C2044
#include "talon/status_macros.h"
int main() { TALON_CONTINUE_IF_ERROR(talon::InternalError("no loop")); }
