// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
// Allocation fault injection is confined to this single-threaded test process.
#include <cstdlib>
#include <new>
#include <string>
#include <utility>

#include "talon/status_or.h"
#include "test.h"

namespace {
bool fail_allocations = false;
struct FailAllocations {
  FailAllocations() { fail_allocations = true; }
  ~FailAllocations() { fail_allocations = false; }
};
struct Counted {
  static int live;
  explicit Counted(int number) noexcept : number(number) { ++live; }
  Counted(const Counted& other) noexcept : number(other.number) { ++live; }
  Counted(Counted&& other) noexcept : number(other.number) { ++live; }
  Counted& operator=(const Counted&) = default;
  Counted& operator=(Counted&&) = default;
  ~Counted() { --live; }
  int number;
};
int Counted::live = 0;
}  // namespace

void* operator new(std::size_t size) {
  if (fail_allocations) throw std::bad_alloc();
  void* memory = std::malloc(size == 0 ? 1 : size);
  if (memory == nullptr) throw std::bad_alloc();
  return memory;
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { ::operator delete(memory); }
#if TALON_STATUS_INTERNAL_LANGUAGE >= 201402L
void operator delete(void* memory, std::size_t) noexcept {
  ::operator delete(memory);
}
void operator delete[](void* memory, std::size_t) noexcept {
  ::operator delete(memory);
}
#endif

int main() {
  talon::StatusOr<int> success(17);
  {
    FailAllocations fail;
    CHECK(success.status()
              .ok());  // First successful status() is allocation-free.
  }
  const talon::Status long_error = talon::NotFoundError(std::string(8192, 'x'));
  talon::Status destination = talon::InternalError("old status");
  const talon::Status original = destination;
  {
    FailAllocations fail;
    talon_test::CheckThrows<std::bad_alloc>([&] { destination = long_error; });
    CHECK_EQ(destination, original);
    talon_test::CheckThrows<std::bad_alloc>(
        [&] { talon::Status copy(long_error); });
  }
  {
    talon::StatusOr<Counted> value(talon::in_place, 17);
    talon::StatusOr<Counted> error(original);
    talon::StatusOr<Counted> long_result(long_error);
    CHECK_EQ(Counted::live, 1);
    {
      FailAllocations fail;
      talon_test::CheckThrows<std::bad_alloc>([&] { value = long_error; });
      CHECK(value.ok());
      CHECK_EQ(value->number, 17);
      CHECK_EQ(Counted::live, 1);
      talon_test::CheckThrows<std::bad_alloc>([&] { value = long_result; });
      CHECK(value.ok());
      CHECK_EQ(Counted::live, 1);
      talon_test::CheckThrows<std::bad_alloc>([&] { error = long_error; });
      CHECK_EQ(error.status(), original);
      talon_test::CheckThrows<std::bad_alloc>([&] { error = long_result; });
      CHECK_EQ(error.status(), original);
      talon_test::CheckThrows<std::bad_alloc>(
          [&] { talon::StatusOr<Counted> copy(long_result); });
      CHECK_EQ(Counted::live, 1);
      // Success construction, move-error commits and error-to-value transitions
      // need no library-side allocation when T itself does not allocate.
      error = value;
      CHECK(error.ok());
      CHECK_EQ(Counted::live, 2);
      value = std::move(long_result);
      CHECK(!value.ok());
      CHECK_EQ(value.status().message().size(), 8192U);
      CHECK_EQ(Counted::live, 1);
    }
  }
  CHECK_EQ(Counted::live, 0);
  return 0;
}
