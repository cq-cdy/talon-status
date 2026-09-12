// EXPECT: ASSIGN_OR_RETURN is already defined
#define ASSIGN_OR_RETURN(left, right) existing_macro(left, right)
#include "talon/status_macros_short.h"
int main() {}
