// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include "workloads.h"

#include <ostream>
#include <string>

#include "talon/status_macros.h"

namespace talon_bench {
namespace {
struct Label {
  int value;
};
std::ostream& operator<<(std::ostream& stream, const Label& label) {
  return stream << "label=" << label.value;
}

talon::StatusOr<int> Chain(unsigned depth, int input, bool fail) {
  if (depth == 0) {
    if (fail) return talon::NotFoundError("leaf: ", input);
    return input;
  }
  TALON_ASSIGN_OR_RETURN(int value, Chain(depth - 1, input, fail));
  TALON_RETURN_IF_ERROR(talon::OkStatus());
  return value + 1;
}
}  // namespace

std::uint64_t Success(std::size_t iterations) {
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < iterations; ++i) {
    talon::StatusOr<int> value(static_cast<int>(i % 10000));
    result += DigestResult(value);
  }
  return result;
}
std::uint64_t LiteralMessage(std::size_t iterations) {
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < iterations; ++i) {
    result += DigestStatus(talon::InvalidArgumentError(
        i % 2 == 0 ? "invalid size" : "invalid item number"));
  }
  return result;
}
std::uint64_t MixedMessage(std::size_t iterations) {
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < iterations; ++i) {
    result += DigestStatus(talon::InvalidArgumentError(
        "invalid size: ", static_cast<int>(i % 10000) - 5000,
        ", expected: ", 1000U));
  }
  return result;
}
std::uint64_t LongMessage(std::size_t iterations) {
  std::uint64_t result = 0;
  std::string message(4096, 'x');
  for (std::size_t i = 0; i < iterations; ++i) {
    message.back() = static_cast<char>('a' + i % 26);
    result += DigestStatus(talon::InternalError(message));
  }
  return result;
}
std::uint64_t CustomMessage(std::size_t iterations) {
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < iterations; ++i) {
    result += DigestStatus(talon::InternalError(
        Label{static_cast<int>(i % 10000)}, ", ratio=", 1.25));
  }
  return result;
}
std::uint64_t ChainSuccess(std::size_t iterations) {
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < iterations; ++i) {
    result += DigestResult(Chain(64, static_cast<int>(i % 10000), false));
  }
  return result;
}
std::uint64_t ChainFailure(std::size_t iterations) {
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < iterations; ++i) {
    result +=
        DigestStatus(Chain(64, static_cast<int>(i % 10000), true).status());
  }
  return result;
}
}  // namespace talon_bench
