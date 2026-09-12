// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_TESTS_TEST_H_
#define TALON_TESTS_TEST_H_

#include <cstdlib>
#include <exception>
#include <iostream>

namespace talon_test {
inline void Check(bool condition, const char* expression, const char* file,
                  int line) {
  if (!condition) {
    std::cerr << file << ':' << line << ": CHECK failed: " << expression << '\n';
    std::abort();
  }
}
template <typename E, typename F>
void CheckThrows(F function) {
  try {
    function();
  } catch (const E&) {
    return;
  } catch (const std::exception& error) {
    std::cerr << "Unexpected exception: " << error.what() << '\n';
    std::abort();
  } catch (...) {
    std::cerr << "Unexpected non-standard exception\n";
    std::abort();
  }
  std::cerr << "Expected exception was not thrown\n";
  std::abort();
}
}  // namespace talon_test

#define CHECK(...) \
  ::talon_test::Check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__, __FILE__, __LINE__)
#define CHECK_EQ(left, right) CHECK((left) == (right))

#endif  // TALON_TESTS_TEST_H_
