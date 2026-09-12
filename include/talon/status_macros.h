// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_STATUS_MACROS_H_
#define TALON_STATUS_MACROS_H_

#include "talon/internal/macro_config.h"

#include <type_traits>
#include <utility>

#include "talon/status_or.h"

namespace talon {
namespace internal {

// The adapter adds specializations, never conditional definitions of the core
// templates. This keeps core-only and adapter-using translation units
// compatible.
template <typename R>
struct StatusTraits;

template <>
struct StatusTraits<Status> {
  typedef Status status_type;
  static const Status& Get(const Status& status) noexcept { return status; }
  static Status&& Get(Status&& status) noexcept { return std::move(status); }
};

template <typename T>
struct StatusTraits<StatusOr<T>> {
  typedef Status status_type;
  static const Status& Get(const StatusOr<T>& result) {
    return result.status();
  }
  static Status Get(StatusOr<T>&& result) { return std::move(result).status(); }
};

template <typename Source, typename Target>
struct StatusConversion;

template <>
struct StatusConversion<Status, Status> {
  static Status Convert(Status status) { return status; }
};

template <typename Target>
struct ErrorReturnTarget {};

template <>
struct ErrorReturnTarget<Status> {
  typedef Status status_type;
  static Status Make(Status status) { return status; }
};

template <typename T>
struct ErrorReturnTarget<StatusOr<T>> {
  typedef Status status_type;
  static StatusOr<T> Make(Status status) {
    return StatusOr<T>(std::move(status));
  }
};

// Own the source status until the return conversion finishes. In particular,
// same-system propagation can retain Abseil payloads without a Talon round
// trip.
template <typename SourceStatus>
class ErrorReturn : public ErrorReturnTag {
 public:
  explicit ErrorReturn(SourceStatus status) : status_(std::move(status)) {}

  template <typename Target,
            typename = typename ErrorReturnTarget<Target>::status_type>
  operator Target() && {  // NOLINT: the macro adapts to the function return
                          // type.
    typedef typename ErrorReturnTarget<Target>::status_type TargetStatus;
    return ErrorReturnTarget<Target>::Make(
        StatusConversion<SourceStatus, TargetStatus>::Convert(
            std::move(status_)));
  }

 private:
  SourceStatus status_;
};

template <typename R>
ErrorReturn<typename StatusTraits<typename std::decay<R>::type>::status_type>
MakeErrorReturn(R&& result) {
  typedef StatusTraits<typename std::decay<R>::type> Traits;
  return ErrorReturn<typename Traits::status_type>(
      Traits::Get(std::forward<R>(result)));
}

template <typename R>
typename StatusTraits<typename std::decay<R>::type>::status_type GetOwnedStatus(
    R&& result) {
  typedef StatusTraits<typename std::decay<R>::type> Traits;
  // Consume the borrowed reference during this call. An expression such as
  // std::move(temporary) or Owner().Get() need not extend the owner's lifetime
  // when bound to a reference in the macro's following declaration.
  if (result.ok()) return typename Traits::status_type();
  return Traits::Get(std::forward<R>(result));
}

}  // namespace internal
}  // namespace talon

#define TALON_STATUS_INTERNAL_CONCAT_IMPL_(a, b) a##b
#define TALON_STATUS_INTERNAL_CONCAT_(a, b) \
  TALON_STATUS_INTERNAL_CONCAT_IMPL_(a, b)

#ifndef TALON_STATUS_USE_LINE_COUNTER
#define TALON_STATUS_USE_LINE_COUNTER 0
#endif

#if TALON_STATUS_USE_LINE_COUNTER != 0 && TALON_STATUS_USE_LINE_COUNTER != 1
#error "TALON_STATUS_USE_LINE_COUNTER must be 0 or 1"
#endif

#if !TALON_STATUS_USE_LINE_COUNTER && defined(__COUNTER__)
#define TALON_STATUS_INTERNAL_UNIQUE_ID_ __COUNTER__
#else
// The fallback does not support two ASSIGN calls on the same physical line.
#define TALON_STATUS_INTERNAL_UNIQUE_ID_ __LINE__
#endif

// Recognize a leading parenthesis and strip one complete surrounding pair.
// A lhs beginning with a partial parenthesized expression must itself be fully
// parenthesized, e.g. ((object).field), not (object).field.
#define TALON_STATUS_INTERNAL_PAREN_PROBE_(...) unused, 1
#define TALON_STATUS_INTERNAL_PAREN_CHECK_IMPL_(unused, result, ...) result
#define TALON_STATUS_INTERNAL_PAREN_CHECK_(...) \
  TALON_STATUS_INTERNAL_PAREN_CHECK_IMPL_(__VA_ARGS__, 0, 0)
#define TALON_STATUS_INTERNAL_IS_PAREN_(x) \
  TALON_STATUS_INTERNAL_PAREN_CHECK_(TALON_STATUS_INTERNAL_PAREN_PROBE_ x)
#define TALON_STATUS_INTERNAL_REMOVE_PARENS_(...) __VA_ARGS__
#define TALON_STATUS_INTERNAL_UNPAREN_0(x) x
#define TALON_STATUS_INTERNAL_UNPAREN_1(x) \
  TALON_STATUS_INTERNAL_REMOVE_PARENS_ x
#define TALON_STATUS_INTERNAL_UNPAREN_(x)                           \
  TALON_STATUS_INTERNAL_CONCAT_(TALON_STATUS_INTERNAL_UNPAREN_,     \
                                TALON_STATUS_INTERNAL_IS_PAREN_(x)) \
  (x)

// Multiple statements are deliberate: a declaration in lhs remains visible in
// the enclosing block. Always use a braced block, including around switch
// cases.
#define TALON_STATUS_INTERNAL_ASSIGN_(lhs, expr, result)          \
  auto result = (expr);                                           \
  if (!result.ok())                                               \
    return ::talon::internal::MakeErrorReturn(std::move(result)); \
  TALON_STATUS_INTERNAL_UNPAREN_(lhs) = std::move(result).value()

#define TALON_ASSIGN_OR_RETURN(lhs, expr)                          \
  TALON_STATUS_INTERNAL_ASSIGN_(                                   \
      lhs, expr,                                                   \
      TALON_STATUS_INTERNAL_CONCAT_(talon_status_internal_result_, \
                                    TALON_STATUS_INTERNAL_UNIQUE_ID_))

#define TALON_STATUS_INTERNAL_RETURN_(expr, result)                 \
  do {                                                              \
    auto result = ::talon::internal::GetOwnedStatus((expr));        \
    if (!result.ok()) {                                             \
      return ::talon::internal::MakeErrorReturn(std::move(result)); \
    }                                                               \
  } while (false)

#define TALON_RETURN_IF_ERROR(expr)                                      \
  TALON_STATUS_INTERNAL_RETURN_(                                         \
      expr, TALON_STATUS_INTERNAL_CONCAT_(talon_status_internal_result_, \
                                          TALON_STATUS_INTERNAL_UNIQUE_ID_))

// Only errors select the labeled statement. A switch avoids a dangling else
// and introduces no loop: continue targets the caller's closest loop. There is
// intentionally no default (success selects nothing). The user's semicolon
// belongs to continue, keeping this a single statement in unbraced if/else.
#define TALON_CONTINUE_IF_ERROR(expr) \
  switch ((expr).ok() ? 0 : 1)        \
  case 1:                             \
    continue

#endif  // TALON_STATUS_MACROS_H_
