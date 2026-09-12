// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include <cstdint>
#include <cstdio>
#include <memory>

#if defined(PROBE_EXPECTED)
#include <expected>
#endif

struct alignas(128) NativeAligned {
  explicit NativeAligned(int number) : value(number) {}
  int value;
};

int main() {
#if defined(PROBE_EXPECTED)
  std::expected<NativeAligned, int> object(std::in_place, 13);
  const NativeAligned* address = std::addressof(object.value());
#else
  NativeAligned object(13);
  const NativeAligned* address = std::addressof(object);
#endif
  const std::uintptr_t remainder =
      reinterpret_cast<std::uintptr_t>(address) % alignof(NativeAligned);
  std::fprintf(stderr, "address=%p alignment=%zu remainder=%zu value=%d\n",
               static_cast<const void*>(address), alignof(NativeAligned),
               static_cast<std::size_t>(remainder), address->value);
  return remainder == 0 && address->value == 13 ? 0 : 1;
}
