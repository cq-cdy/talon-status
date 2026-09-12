// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include <cstdlib>
#include <expected>
#include <iostream>
#include <string>

namespace probe {
inline const std::string& SharedEmpty() {
  static const std::string* const value = new std::string();
  return *value;
}

struct Wrapper {
  std::expected<int, std::string> data;
  explicit Wrapper(int value) noexcept : data(std::in_place, value) {}
  const std::string& status() const {
    return data.has_value() ? SharedEmpty() : data.error();
  }
};

inline void Check(bool value) {
  if (!value) {
    std::cerr << "failed\n";
    std::abort();
  }
}
}  // namespace probe

namespace {
struct CheckAtExit {
  ~CheckAtExit() {
    probe::Wrapper success(7);
    probe::Check(success.status().empty());
    probe::Check(success.status().empty());
    probe::Check(success.status() == std::string());
  }
} checker;
}  // namespace

int main() {
  probe::Wrapper success(7);
  probe::Check(success.status().empty());
  return 0;
}
