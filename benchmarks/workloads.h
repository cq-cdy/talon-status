// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_BENCHMARKS_WORKLOADS_H_
#define TALON_BENCHMARKS_WORKLOADS_H_
#include <cstddef>
#include <cstdint>

namespace talon {
class Status;
template <typename T>
class StatusOr;
}  // namespace talon

namespace talon_bench {
typedef std::uint64_t (*Workload)(std::size_t);
// Implemented in another translation unit to keep construction observable
// without compiler-specific barriers. Benchmark with LTO disabled.
std::uint64_t DigestStatus(const talon::Status& status);
std::uint64_t DigestResult(const talon::StatusOr<int>& result);
std::uint64_t Success(std::size_t iterations);
std::uint64_t LiteralMessage(std::size_t iterations);
std::uint64_t MixedMessage(std::size_t iterations);
std::uint64_t LongMessage(std::size_t iterations);
std::uint64_t CustomMessage(std::size_t iterations);
std::uint64_t ChainSuccess(std::size_t iterations);
std::uint64_t ChainFailure(std::size_t iterations);
}  // namespace talon_bench
#endif  // TALON_BENCHMARKS_WORKLOADS_H_
