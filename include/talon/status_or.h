// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_STATUS_OR_H_
#define TALON_STATUS_OR_H_

#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "talon/config.h"
#include "talon/internal/special_members.h"
#include "talon/status.h"
#if !TALON_STATUS_USE_STD_EXPECTED
#include "talon/internal/compat_storage.h"
#endif

namespace talon {

struct in_place_t {
  explicit constexpr in_place_t() {}
};
constexpr in_place_t in_place{};

template <typename T>
class StatusOr;

class BadStatusOrAccess : public std::logic_error {
 public:
  explicit BadStatusOrAccess(const Status& status)
      : std::logic_error(status.ToString()), status_(status) {}
  const Status& status() const noexcept { return status_; }

 private:
  Status status_;
};

namespace internal {

struct ErrorReturnTag {};
template <typename T>
struct IsStatusOr : std::false_type {};
template <typename T>
struct IsStatusOr<StatusOr<T>> : std::true_type {};

template <typename T>
struct IsExpectedTag : std::false_type {};
#if TALON_STATUS_INTERNAL_LANGUAGE >= 201703L
template <>
struct IsExpectedTag<std::in_place_t> : std::true_type {};
#endif
#if TALON_STATUS_HAS_STD_EXPECTED
template <>
struct IsExpectedTag<std::unexpect_t> : std::true_type {};
template <typename E>
struct IsExpectedTag<std::unexpected<E>> : std::true_type {};
#endif

// expected's assignment overloads interpret native expected/unexpected inputs
// as result state. Do not let a greedy T (e.g. any) accidentally select those
// overloads instead of a value assignment. Use in_place or construct T
// explicitly.
template <typename T>
struct IsNativeResultSource : IsExpectedTag<T> {};
#if TALON_STATUS_HAS_STD_EXPECTED
template <typename V, typename E>
struct IsNativeResultSource<std::expected<V, E>> : std::true_type {};
#endif

template <typename T>
struct IsSupportedValue
    : std::integral_constant<
          bool, std::is_object<T>::value && !std::is_array<T>::value &&
                    !std::is_const<T>::value && !std::is_volatile<T>::value &&
                    !std::is_same<T, Status>::value &&
                    !std::is_same<T, in_place_t>::value &&
                    !IsExpectedTag<T>::value &&
                    std::is_nothrow_destructible<T>::value> {};

template <typename T, typename U>
struct IsValueSource
    : std::integral_constant<
          bool,
          !std::is_same<typename std::decay<U>::type, Status>::value &&
              !std::is_same<typename std::decay<U>::type, in_place_t>::value &&
              !IsStatusOr<typename std::decay<U>::type>::value &&
              !IsNativeResultSource<typename std::decay<U>::type>::value &&
              !std::is_base_of<ErrorReturnTag,
                               typename std::decay<U>::type>::value &&
              std::is_constructible<T, U>::value> {};

inline Status CheckError(Status status) {
  if (status.ok()) {
    throw std::invalid_argument("StatusOr error must not be OK");
  }
  return status;
}

inline const Status& SharedOkStatus() noexcept {
  // Trivial static storage is never registered for destruction. Keeping the
  // placement-new pointer also avoids a dependency on std::launder in C++11.
  // This allocation-free OK object remains usable from global destructors.
  alignas(Status) static unsigned char storage[sizeof(Status)];
  static const Status* const status =
      ::new (static_cast<void*>(storage)) Status();
  return *status;
}

template <typename T>
struct ResultStorage {
  // Avoid instantiating a standard-library specialization with an illegal T;
  // the public assertion supplies the primary diagnostic.
  typedef typename std::conditional<IsSupportedValue<T>::value, T, int>::type V;
#if TALON_STATUS_USE_STD_EXPECTED
  typedef std::expected<V, Status> type;
#else
  typedef CompatStorage<V> type;
#endif
};

}  // namespace internal

template <typename T>
class StatusOr
    : private internal::CopyConstructible<std::is_copy_constructible<T>::value>,
      private internal::MoveConstructible<std::is_move_constructible<T>::value>,
      private internal::CopyAssignable<std::is_copy_constructible<T>::value &&
                                       std::is_copy_assignable<T>::value>,
      private internal::MoveAssignable<std::is_move_constructible<T>::value &&
                                       std::is_move_assignable<T>::value> {
  static_assert(
      internal::IsSupportedValue<T>::value,
      "StatusOr requires a complete non-cv object value with noexcept "
      "destructor; void, references, arrays, Status and tags are unsupported");

 public:
  typedef T value_type;
  typedef Status error_type;

  explicit StatusOr() : StatusOr(UnknownError()) {}
  StatusOr(const StatusOr&) = default;
  StatusOr(StatusOr&&) = default;
  StatusOr& operator=(const StatusOr&) = default;
  StatusOr& operator=(StatusOr&&) = default;

  template <typename S,
            typename std::enable_if<
                std::is_same<typename std::decay<S>::type, Status>::value &&
                    std::is_constructible<Status, S&&>::value,
                int>::type = 0>
  StatusOr(S&& status)  // Intentional implicit error conversion.
      : data_(
#if TALON_STATUS_USE_STD_EXPECTED
            std::unexpect,
#else
            internal::ErrorTag(),
#endif
            internal::CheckError(std::forward<S>(status))) {
  }

  template <typename... Args,
            typename std::enable_if<std::is_constructible<T, Args&&...>::value,
                                    int>::type = 0>
  explicit StatusOr(in_place_t, Args&&... args) noexcept(
      std::is_nothrow_constructible<T, Args&&...>::value)
      : data_(
#if TALON_STATUS_USE_STD_EXPECTED
            std::in_place,
#else
            internal::ValueTag(),
#endif
            std::forward<Args>(args)...) {
  }

  template <typename U,
            typename std::enable_if<internal::IsValueSource<T, U&&>::value &&
                                        std::is_convertible<U&&, T>::value,
                                    int>::type = 0>
  StatusOr(U&& value)  // Follows the implicitness of U -> T.
      noexcept(std::is_nothrow_constructible<T, U&&>::value)
      : StatusOr(in_place, std::forward<U>(value)) {}

  template <typename U,
            typename std::enable_if<internal::IsValueSource<T, U&&>::value &&
                                        !std::is_convertible<U&&, T>::value,
                                    int>::type = 0>
  explicit StatusOr(U&& value) noexcept(
      std::is_nothrow_constructible<T, U&&>::value)
      : StatusOr(in_place, std::forward<U>(value)) {}

  template <typename S>
  typename std::enable_if<
      std::is_same<typename std::decay<S>::type, Status>::value &&
          std::is_constructible<Status, S&&>::value,
      StatusOr&>::type
  operator=(S&& status) {
    Status error = internal::CheckError(std::forward<S>(status));
#if TALON_STATUS_USE_STD_EXPECTED
    data_ = std::unexpected<Status>(std::move(error));
#else
    data_.AssignError(std::move(error));
#endif
    return *this;
  }

  template <typename U>
  typename std::enable_if<internal::IsValueSource<T, U&&>::value &&
                              std::is_assignable<T&, U&&>::value,
                          StatusOr&>::type
  operator=(U&& value) {
#if TALON_STATUS_USE_STD_EXPECTED
    data_ = std::forward<U>(value);
#else
    data_.AssignValue(std::forward<U>(value));
#endif
    return *this;
  }

  bool ok() const noexcept { return data_.has_value(); }
  const Status& status() const& noexcept {
    return ok() ? internal::SharedOkStatus() : data_.error();
  }
  Status status() && noexcept {
    return ok() ? OkStatus() : std::move(data_).error();
  }
  Status status() const&& { return status(); }

  T& value() & {
    CheckValue();
    return *data_;
  }
  const T& value() const& {
    CheckValue();
    return *data_;
  }
  T&& value() && {
    CheckValue();
    return *std::move(data_);
  }
  const T&& value() const&& {
    CheckValue();
    return *std::move(data_);
  }
  T& operator*() & { return value(); }
  const T& operator*() const& { return value(); }
  T&& operator*() && { return std::move(*this).value(); }
  const T&& operator*() const&& { return std::move(*this).value(); }
  T* operator->() { return std::addressof(value()); }
  const T* operator->() const { return std::addressof(value()); }

 private:
  void CheckValue() const {
    if (!ok()) throw BadStatusOrAccess(data_.error());
  }

  typename internal::ResultStorage<T>::type data_;
};

}  // namespace talon

#endif  // TALON_STATUS_OR_H_
