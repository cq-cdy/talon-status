// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_ABSL_ADAPTER_H_
#define TALON_ABSL_ADAPTER_H_

#include <string>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/utility/utility.h"
#include "talon/status_macros.h"

namespace talon {
namespace internal {

inline StatusCode FromAbslCode(absl::StatusCode code) noexcept {
  switch (code) {
    case absl::StatusCode::kOk: return StatusCode::kOk;
    case absl::StatusCode::kCancelled: return StatusCode::kCancelled;
    case absl::StatusCode::kUnknown: return StatusCode::kUnknown;
    case absl::StatusCode::kInvalidArgument: return StatusCode::kInvalidArgument;
    case absl::StatusCode::kDeadlineExceeded: return StatusCode::kDeadlineExceeded;
    case absl::StatusCode::kNotFound: return StatusCode::kNotFound;
    case absl::StatusCode::kAlreadyExists: return StatusCode::kAlreadyExists;
    case absl::StatusCode::kPermissionDenied: return StatusCode::kPermissionDenied;
    case absl::StatusCode::kResourceExhausted: return StatusCode::kResourceExhausted;
    case absl::StatusCode::kFailedPrecondition:
      return StatusCode::kFailedPrecondition;
    case absl::StatusCode::kAborted: return StatusCode::kAborted;
    case absl::StatusCode::kOutOfRange: return StatusCode::kOutOfRange;
    case absl::StatusCode::kUnimplemented: return StatusCode::kUnimplemented;
    case absl::StatusCode::kInternal: return StatusCode::kInternal;
    case absl::StatusCode::kUnavailable: return StatusCode::kUnavailable;
    case absl::StatusCode::kDataLoss: return StatusCode::kDataLoss;
    case absl::StatusCode::kUnauthenticated: return StatusCode::kUnauthenticated;
    default: return StatusCode::kUnknown;
  }
}

inline absl::StatusCode ToAbslCode(StatusCode code) noexcept {
  switch (code) {
    case StatusCode::kOk: return absl::StatusCode::kOk;
    case StatusCode::kCancelled: return absl::StatusCode::kCancelled;
    case StatusCode::kUnknown: return absl::StatusCode::kUnknown;
    case StatusCode::kInvalidArgument: return absl::StatusCode::kInvalidArgument;
    case StatusCode::kDeadlineExceeded: return absl::StatusCode::kDeadlineExceeded;
    case StatusCode::kNotFound: return absl::StatusCode::kNotFound;
    case StatusCode::kAlreadyExists: return absl::StatusCode::kAlreadyExists;
    case StatusCode::kPermissionDenied: return absl::StatusCode::kPermissionDenied;
    case StatusCode::kResourceExhausted: return absl::StatusCode::kResourceExhausted;
    case StatusCode::kFailedPrecondition:
      return absl::StatusCode::kFailedPrecondition;
    case StatusCode::kAborted: return absl::StatusCode::kAborted;
    case StatusCode::kOutOfRange: return absl::StatusCode::kOutOfRange;
    case StatusCode::kUnimplemented: return absl::StatusCode::kUnimplemented;
    case StatusCode::kInternal: return absl::StatusCode::kInternal;
    case StatusCode::kUnavailable: return absl::StatusCode::kUnavailable;
    case StatusCode::kDataLoss: return absl::StatusCode::kDataLoss;
    case StatusCode::kUnauthenticated: return absl::StatusCode::kUnauthenticated;
    default: return absl::StatusCode::kUnknown;
  }
}

}  // namespace internal

// Cross-system conversion owns a copy of the complete message, including NUL
// bytes. Talon has no payload facility, so Abseil payloads are not preserved.
inline Status ToTalonStatus(const absl::Status& status) {
  if (status.ok()) return OkStatus();
  const absl::string_view message = status.message();
  // Abseil represents an empty inlined error message with a null data pointer.
  return Status(internal::FromAbslCode(status.code()),
                message.empty() ? std::string()
                                : std::string(message.data(), message.size()));
}

inline absl::Status ToAbslStatus(const Status& status) {
  return absl::Status(internal::ToAbslCode(status.code()),
                      absl::string_view(status.message().data(),
                                        status.message().size()));
}

template <typename T>
typename std::enable_if<std::is_copy_constructible<T>::value,
                        StatusOr<T> >::type
ToTalonStatusOr(const absl::StatusOr<T>& result) {
  if (!result.ok()) return ToTalonStatus(result.status());
  return StatusOr<T>(in_place, result.value());
}

template <typename T>
typename std::enable_if<std::is_move_constructible<T>::value,
                        StatusOr<T> >::type
ToTalonStatusOr(absl::StatusOr<T>&& result) {
  if (!result.ok()) return ToTalonStatus(std::move(result).status());
  return StatusOr<T>(in_place, std::move(result).value());
}

template <typename T>
typename std::enable_if<std::is_copy_constructible<T>::value,
                        absl::StatusOr<T> >::type
ToAbslStatusOr(const StatusOr<T>& result) {
  if (!result.ok()) return ToAbslStatus(result.status());
  return absl::StatusOr<T>(absl::in_place, result.value());
}

template <typename T>
typename std::enable_if<std::is_move_constructible<T>::value,
                        absl::StatusOr<T> >::type
ToAbslStatusOr(StatusOr<T>&& result) {
  if (!result.ok()) return ToAbslStatus(std::move(result).status());
  return absl::StatusOr<T>(absl::in_place, std::move(result).value());
}

namespace internal {

template <>
struct StatusTraits<absl::Status> {
  typedef absl::Status status_type;
  static const absl::Status& Get(const absl::Status& status) { return status; }
  static absl::Status&& Get(absl::Status&& status) { return std::move(status); }
};

template <typename T>
struct StatusTraits<absl::StatusOr<T> > {
  typedef absl::Status status_type;
  static const absl::Status& Get(const absl::StatusOr<T>& result) {
    return result.status();
  }
  static absl::Status Get(absl::StatusOr<T>&& result) {
    return std::move(result).status();
  }
};

template <>
struct StatusConversion<Status, absl::Status> {
  static absl::Status Convert(Status status) { return ToAbslStatus(status); }
};

template <>
struct StatusConversion<absl::Status, Status> {
  static Status Convert(absl::Status status) { return ToTalonStatus(status); }
};

template <>
struct StatusConversion<absl::Status, absl::Status> {
  static absl::Status Convert(absl::Status status) { return status; }
};

template <>
struct ErrorReturnTarget<absl::Status> {
  typedef absl::Status status_type;
  static absl::Status Make(absl::Status status) { return status; }
};

template <typename T>
struct ErrorReturnTarget<absl::StatusOr<T> > {
  typedef absl::Status status_type;
  static absl::StatusOr<T> Make(absl::Status status) {
    return absl::StatusOr<T>(std::move(status));
  }
};

}  // namespace internal
}  // namespace talon

#endif  // TALON_ABSL_ADAPTER_H_
