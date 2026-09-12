// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_INTERNAL_COMPAT_STORAGE_H_
#define TALON_INTERNAL_COMPAT_STORAGE_H_

#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "talon/status.h"

namespace talon {
namespace internal {

struct ValueTag {};
struct ErrorTag {};

// This is private storage, not a public implementation of std::expected.
// Status always lives. T lives exactly when status_.ok() is true.
// StatusOr gates special members before their bodies can be instantiated.
template <typename T>
class CompatStorage {
 public:
  template <typename... Args>
  explicit CompatStorage(ValueTag, Args&&... args) noexcept(
      std::is_nothrow_constructible<T, Args&&...>::value)
      : status_(), value_(), object_(nullptr) {
    Construct(std::forward<Args>(args)...);
  }
  explicit CompatStorage(ErrorTag, Status status) noexcept
      : status_(std::move(status)), value_(), object_(nullptr) {}

  CompatStorage(const CompatStorage& other)
      : status_(other.status_), value_(), object_(nullptr) {
    if (has_value()) Construct(*other);
  }
  CompatStorage(CompatStorage&& other) noexcept(
      std::is_nothrow_move_constructible<T>::value)
      : status_(std::move(other.status_)), value_(), object_(nullptr) {
    if (has_value()) Construct(*std::move(other));
  }
  CompatStorage& operator=(const CompatStorage& other) {
    if (this != std::addressof(other)) {
      if (other.has_value()) {
        AssignValue(*other);
      } else {
        AssignError(other.status_);
      }
    }
    return *this;
  }
  CompatStorage& operator=(CompatStorage&& other) noexcept(
      std::is_nothrow_move_constructible<T>::value&&
          std::is_nothrow_move_assignable<T>::value) {
    if (this != std::addressof(other)) {
      if (other.has_value()) {
        AssignValue(*std::move(other));
      } else {
        AssignError(std::move(other.status_));
      }
    }
    return *this;
  }
  ~CompatStorage() {
    if (has_value()) object_->~T();
  }

  bool has_value() const noexcept { return status_.ok(); }
  const Status& error() const& noexcept { return status_; }
  Status& error() & noexcept { return status_; }
  Status&& error() && noexcept { return std::move(status_); }
  const T& operator*() const& noexcept { return *object_; }
  T& operator*() & noexcept { return *object_; }
  const T&& operator*() const&& noexcept { return std::move(*object_); }
  T&& operator*() && noexcept { return std::move(*object_); }

  template <typename U>
  void AssignValue(U&& value) {
    if (has_value()) {
      *object_ = std::forward<U>(value);
    } else {
      // If construction throws, the previous error and lifetime remain intact.
      Construct(std::forward<U>(value));
      status_ = OkStatus();
    }
  }
  void AssignError(Status error) noexcept {
    // Preparing the by-value argument happens before any modification here.
    if (has_value()) object_->~T();
    object_ = nullptr;
    status_ = std::move(error);
  }

 private:
  template <typename... Args>
  void Construct(Args&&... args) {
    object_ = ::new (static_cast<void*>(std::addressof(value_.object)))
        T(std::forward<Args>(args)...);
  }

  Status status_;
  union ValueStorage {
    unsigned char empty;
    T object;
    ValueStorage() noexcept : empty(0) {}
    ~ValueStorage() {}
  } value_;
  // Keep the fresh placement-new pointer: pre-C++20 transparent replacement
  // rules are stricter for T with const/reference members (see P0137R1).
  T* object_;
};

}  // namespace internal
}  // namespace talon

#endif  // TALON_INTERNAL_COMPAT_STORAGE_H_
