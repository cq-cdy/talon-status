// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "talon/status_or.h"
#include "workloads.h"

namespace {
volatile std::uint64_t benchmark_sink = 0;
struct Case {
  const char* name;
  talon_bench::Workload run;
};
std::size_t ParseCount(const char* argument) {
  const std::string text(argument);
  if (text.empty() ||
      text.find_first_not_of("0123456789") != std::string::npos) {
    throw std::invalid_argument("counts must contain decimal digits only");
  }
  const unsigned long value = std::stoul(text);
  if (value > (std::numeric_limits<std::size_t>::max)()) {
    throw std::out_of_range("count does not fit size_t");
  }
  return static_cast<std::size_t>(value);
}
}  // namespace

namespace talon_bench {
std::uint64_t DigestStatus(const talon::Status& status) {
  const std::string& message = status.message();
  return static_cast<std::uint64_t>(status.code()) + message.size() +
         (message.empty() ? 0U : static_cast<unsigned char>(message.back()));
}
std::uint64_t DigestResult(const talon::StatusOr<int>& result) {
  return static_cast<std::uint64_t>(result.value());
}
}  // namespace talon_bench

int main(int argc, char** argv) {
  try {
    const std::size_t iterations = argc > 1 ? ParseCount(argv[1]) : 30000;
    const std::size_t requested_repeats = argc > 2 ? ParseCount(argv[2]) : 7;
    if (argc > 3 || iterations == 0 || requested_repeats == 0 ||
        requested_repeats > 100) {
      throw std::invalid_argument(
          "iterations must be positive; repeats must be 1..100");
    }
    const unsigned repeats = static_cast<unsigned>(requested_repeats);
    const Case cases[] = {{"success", talon_bench::Success},
                          {"literal_message", talon_bench::LiteralMessage},
                          {"mixed_message", talon_bench::MixedMessage},
                          {"long_message", talon_bench::LongMessage},
                          {"custom_message", talon_bench::CustomMessage},
                          {"chain_success_64", talon_bench::ChainSuccess},
                          {"chain_failure_64", talon_bench::ChainFailure}};
    std::cout << "backend,"
              << (TALON_STATUS_USE_STD_EXPECTED ? "STD" : "COMPAT")
              << "\ncase,iterations,repeats,median_ns_per_operation,checksum\n";
    for (const Case& entry : cases) {
      const std::uint64_t expected =
          entry.run(iterations);  // Warm up and check determinism.
      std::vector<double> samples;
      for (unsigned trial = 0; trial < repeats; ++trial) {
        const auto begin = std::chrono::steady_clock::now();
        const std::uint64_t checksum = entry.run(iterations);
        const auto end = std::chrono::steady_clock::now();
        if (checksum != expected)
          throw std::runtime_error("non-deterministic workload");
        benchmark_sink = benchmark_sink ^ checksum;
        samples.push_back(
            std::chrono::duration<double, std::nano>(end - begin).count() /
            static_cast<double>(iterations));
      }
      std::sort(samples.begin(), samples.end());
      std::cout << entry.name << ',' << iterations << ',' << repeats << ','
                << std::fixed << std::setprecision(2)
                << samples[samples.size() / 2] << ',' << expected << '\n';
    }
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
