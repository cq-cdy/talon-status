// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_STATUS_H_
#define TALON_STATUS_H_

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "talon/config.h"
#include "talon/internal/message.h"

namespace talon {

enum class StatusCode : int {
  kOk = 0,
  kCancelled = 1,
  kUnknown = 2,
  kInvalidArgument = 3,
  kDeadlineExceeded = 4,
  kNotFound = 5,
  kAlreadyExists = 6,
  kPermissionDenied = 7,
  kResourceExhausted = 8,
  kFailedPrecondition = 9,
  kAborted = 10,
  kOutOfRange = 11,
  kUnimplemented = 12,
  kInternal = 13,
  kUnavailable = 14,
  kDataLoss = 15,
  kUnauthenticated = 16
};

inline const char* StatusCodeToString(StatusCode code) noexcept {
  switch (code) {
    case StatusCode::kOk:
      return "OK";
    case StatusCode::kCancelled:
      return "CANCELLED";
    case StatusCode::kUnknown:
      return "UNKNOWN";
    case StatusCode::kInvalidArgument:
      return "INVALID_ARGUMENT";
    case StatusCode::kDeadlineExceeded:
      return "DEADLINE_EXCEEDED";
    case StatusCode::kNotFound:
      return "NOT_FOUND";
    case StatusCode::kAlreadyExists:
      return "ALREADY_EXISTS";
    case StatusCode::kPermissionDenied:
      return "PERMISSION_DENIED";
    case StatusCode::kResourceExhausted:
      return "RESOURCE_EXHAUSTED";
    case StatusCode::kFailedPrecondition:
      return "FAILED_PRECONDITION";
    case StatusCode::kAborted:
      return "ABORTED";
    case StatusCode::kOutOfRange:
      return "OUT_OF_RANGE";
    case StatusCode::kUnimplemented:
      return "UNIMPLEMENTED";
    case StatusCode::kInternal:
      return "INTERNAL";
    case StatusCode::kUnavailable:
      return "UNAVAILABLE";
    case StatusCode::kDataLoss:
      return "DATA_LOSS";
    case StatusCode::kUnauthenticated:
      return "UNAUTHENTICATED";
  }
  return "UNKNOWN";
}

class Status {
 public:
  Status() noexcept : code_(StatusCode::kOk), message_() {}

  Status(StatusCode code, std::string message)
      : code_(NormalizeCode(code)),
        message_(code_ == StatusCode::kOk ? std::string()
                                          : std::move(message)) {}

  Status(StatusCode code, const char* message)
      : Status(code, std::string(message == nullptr ? "(null)" : message)) {}

  Status(const Status&) = default;
  Status(Status&& other) noexcept
      : code_(other.code_), message_(std::move(other.message_)) {
    other.message_.clear();
  }

  Status& operator=(const Status& other) {
    if (this != &other) {
      Status copy(other);
      swap(copy);
    }
    return *this;
  }
  Status& operator=(Status&& other) noexcept {
    if (this != &other) {
      code_ = other.code_;
      message_ = std::move(other.message_);
      other.message_.clear();
    }
    return *this;
  }

  bool ok() const noexcept { return code_ == StatusCode::kOk; }
  StatusCode code() const noexcept { return code_; }
  const std::string& message() const noexcept { return message_; }
  std::string ToString() const {
    const std::string name(StatusCodeToString(code_));
    return message_.empty() ? name : name + ": " + message_;
  }
  void swap(Status& other) noexcept {
    using std::swap;
    swap(code_, other.code_);
    message_.swap(other.message_);
  }

 private:
  static StatusCode NormalizeCode(StatusCode code) noexcept {
    const int number = static_cast<int>(code);
    return number >= 0 && number <= 16 ? code : StatusCode::kUnknown;
  }

  StatusCode code_;
  std::string message_;
};

static_assert(std::is_nothrow_default_constructible<std::string>::value &&
                  std::is_nothrow_move_constructible<std::string>::value &&
                  std::is_nothrow_move_assignable<std::string>::value,
              "Talon requires noexcept standard-allocator string operations");

inline bool operator==(const Status& left, const Status& right) noexcept {
  return left.code() == right.code() && left.message() == right.message();
}
inline bool operator!=(const Status& left, const Status& right) noexcept {
  return !(left == right);
}
inline void swap(Status& left, Status& right) noexcept { left.swap(right); }
inline std::ostream& operator<<(std::ostream& stream, const Status& status) {
  return stream << status.ToString();
}
inline Status OkStatus() noexcept { return Status(); }

#define TALON_STATUS_INTERNAL_FACTORY(name, code_name)                    \
  template <typename... Args>                                             \
  Status name(const Args&... args) {                                      \
    return Status(StatusCode::code_name, internal::MakeMessage(args...)); \
  }

TALON_STATUS_INTERNAL_FACTORY(CancelledError, kCancelled)
TALON_STATUS_INTERNAL_FACTORY(UnknownError, kUnknown)
TALON_STATUS_INTERNAL_FACTORY(InvalidArgumentError, kInvalidArgument)
TALON_STATUS_INTERNAL_FACTORY(DeadlineExceededError, kDeadlineExceeded)
TALON_STATUS_INTERNAL_FACTORY(NotFoundError, kNotFound)
TALON_STATUS_INTERNAL_FACTORY(AlreadyExistsError, kAlreadyExists)
TALON_STATUS_INTERNAL_FACTORY(PermissionDeniedError, kPermissionDenied)
TALON_STATUS_INTERNAL_FACTORY(ResourceExhaustedError, kResourceExhausted)
TALON_STATUS_INTERNAL_FACTORY(FailedPreconditionError, kFailedPrecondition)
TALON_STATUS_INTERNAL_FACTORY(AbortedError, kAborted)
TALON_STATUS_INTERNAL_FACTORY(OutOfRangeError, kOutOfRange)
TALON_STATUS_INTERNAL_FACTORY(UnimplementedError, kUnimplemented)
TALON_STATUS_INTERNAL_FACTORY(InternalError, kInternal)
TALON_STATUS_INTERNAL_FACTORY(UnavailableError, kUnavailable)
TALON_STATUS_INTERNAL_FACTORY(DataLossError, kDataLoss)
TALON_STATUS_INTERNAL_FACTORY(UnauthenticatedError, kUnauthenticated)

#undef TALON_STATUS_INTERNAL_FACTORY

}  // namespace talon

#endif  // TALON_STATUS_H_
