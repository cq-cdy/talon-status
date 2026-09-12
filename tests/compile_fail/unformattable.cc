// EXPECT: Talon message arguments must support stream insertion
#include "talon/status.h"
struct Unformattable {};
int main() { (void)talon::InternalError(Unformattable()); }
