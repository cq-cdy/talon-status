// EXPECT: no matching|no viable|cannot convert|C2665|C2440
#include "talon/status_or.h"
int main() {
  talon::StatusOr<int> source(1);
  talon::StatusOr<long> value(source);
}
