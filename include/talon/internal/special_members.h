// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_INTERNAL_SPECIAL_MEMBERS_H_
#define TALON_INTERNAL_SPECIAL_MEMBERS_H_

namespace talon {
namespace internal {

// Independently gate the four defaulted special members. A deleted defaulted
// move is ignored by overload resolution, allowing the standard copy fallback.
template <bool Enabled>
struct CopyConstructible {};
template <>
struct CopyConstructible<false> {
  CopyConstructible() = default;
  CopyConstructible(const CopyConstructible&) = delete;
  CopyConstructible(CopyConstructible&&) = default;
  CopyConstructible& operator=(const CopyConstructible&) = default;
  CopyConstructible& operator=(CopyConstructible&&) = default;
};

template <bool Enabled>
struct MoveConstructible {};
template <>
struct MoveConstructible<false> {
  MoveConstructible() = default;
  MoveConstructible(const MoveConstructible&) = default;
  MoveConstructible(MoveConstructible&&) = delete;
  MoveConstructible& operator=(const MoveConstructible&) = default;
  MoveConstructible& operator=(MoveConstructible&&) = default;
};

template <bool Enabled>
struct CopyAssignable {};
template <>
struct CopyAssignable<false> {
  CopyAssignable() = default;
  CopyAssignable(const CopyAssignable&) = default;
  CopyAssignable(CopyAssignable&&) = default;
  CopyAssignable& operator=(const CopyAssignable&) = delete;
  CopyAssignable& operator=(CopyAssignable&&) = default;
};

template <bool Enabled>
struct MoveAssignable {};
template <>
struct MoveAssignable<false> {
  MoveAssignable() = default;
  MoveAssignable(const MoveAssignable&) = default;
  MoveAssignable(MoveAssignable&&) = default;
  MoveAssignable& operator=(const MoveAssignable&) = default;
  MoveAssignable& operator=(MoveAssignable&&) = delete;
};

}  // namespace internal
}  // namespace talon

#endif  // TALON_INTERNAL_SPECIAL_MEMBERS_H_
