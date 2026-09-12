// EXPECT: deleted|C2280
#include <memory>
#include "talon/status_or.h"
int main() {
  talon::StatusOr<std::unique_ptr<int> > value(std::unique_ptr<int>(new int(1)));
  talon::StatusOr<std::unique_ptr<int> > copy(value);
}
