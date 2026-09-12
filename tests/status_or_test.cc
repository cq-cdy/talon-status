// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include "talon/status_or.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "test.h"

namespace {
using talon::StatusOr;

struct CheckStatusAtExit {
  // Keep the successful result alive across main and static teardown. This also
  // avoids GCC12's O3 false positive for a local expected in this destructor;
  // tests/toolchain/gcc12_expected_warning.cc reproduces it without Talon.
  CheckStatusAtExit() : success(7) {}
  ~CheckStatusAtExit() {
    CHECK(success.status().ok());
    CHECK(success.status().message().empty());
    CHECK_EQ(success.status().ToString(), "OK");
  }
  StatusOr<int> success;
} check_status_at_exit;

struct NoDefault {
  explicit NoDefault(int number) noexcept : number(number) {}
  int number;
};
struct CopyOnly {
  explicit CopyOnly(int value) : value(value) {}
  CopyOnly(const CopyOnly&) = default;
  CopyOnly(CopyOnly&&) = delete;
  CopyOnly& operator=(const CopyOnly&) = default;
  CopyOnly& operator=(CopyOnly&&) = delete;
  int value;
};
struct CopyConstructOnly {
  CopyConstructOnly() = default;
  CopyConstructOnly(const CopyConstructOnly&) = default;
  CopyConstructOnly& operator=(const CopyConstructOnly&) = delete;
};
struct MoveConstructOnly {
  MoveConstructOnly() = default;
  MoveConstructOnly(MoveConstructOnly&&) noexcept = default;
  MoveConstructOnly& operator=(MoveConstructOnly&&) = delete;
};
struct Immobile {
  explicit Immobile(int number) : number(number) {}
  Immobile(const Immobile&) = delete;
  Immobile(Immobile&&) = delete;
  Immobile& operator=(const Immobile&) = delete;
  Immobile& operator=(Immobile&&) = delete;
  int number;
};
struct AssignOnly {
  AssignOnly() = default;
  AssignOnly(const AssignOnly&) = delete;
  AssignOnly(AssignOnly&&) = delete;
  AssignOnly& operator=(const AssignOnly&) = default;
  AssignOnly& operator=(AssignOnly&&) = default;
};
struct Implicit {
  Implicit(int value) : value(value) {}
  int value;
};
struct StatusLike {
  operator talon::Status() const { return talon::InternalError("conversion"); }
};
struct alignas(128) OverAligned {
  explicit OverAligned(int value)
      : value(static_cast<std::uint32_t>(value)), alignment_padding() {}
  OverAligned* operator&() { return nullptr; }
  const OverAligned* operator&() const { return nullptr; }
  std::uint32_t value;
  unsigned char alignment_padding[124];
};
static_assert(sizeof(OverAligned) == 128,
              "over-aligned test value must not need tail padding");

struct ConstMember {
  explicit ConstMember(int value) : value(value) {}
  ConstMember(const ConstMember&) = default;
  ConstMember& operator=(const ConstMember&) { return *this; }
  const int value;
};
struct ReferenceMember {
  explicit ReferenceMember(int& value) : value(value) {}
  ReferenceMember(const ReferenceMember&) = default;
  ReferenceMember& operator=(const ReferenceMember& other) {
    value = other.value;
    return *this;
  }
  int& value;
};

static_assert(std::is_default_constructible<StatusOr<NoDefault>>::value,
              "default result does not default-construct T");
static_assert(
    !std::is_copy_constructible<StatusOr<std::unique_ptr<int>>>::value,
    "move-only values cannot be copied");
static_assert(!std::is_copy_assignable<StatusOr<std::unique_ptr<int>>>::value,
              "move-only values cannot be copy-assigned");
static_assert(
    std::is_nothrow_move_constructible<StatusOr<std::unique_ptr<int>>>::value,
    "unique_ptr results move without throwing");
static_assert(
    std::is_nothrow_move_assignable<StatusOr<std::unique_ptr<int>>>::value,
    "unique_ptr results move-assign without throwing");
static_assert(std::is_copy_constructible<StatusOr<CopyConstructOnly>>::value,
              "copy construction is independent of assignment");
static_assert(!std::is_copy_assignable<StatusOr<CopyConstructOnly>>::value,
              "assignment requires T assignment");
static_assert(std::is_move_constructible<StatusOr<MoveConstructOnly>>::value,
              "move construction is independent of assignment");
static_assert(!std::is_move_assignable<StatusOr<MoveConstructOnly>>::value,
              "move assignment requires T assignment");
static_assert(!std::is_copy_constructible<StatusOr<Immobile>>::value &&
                  !std::is_move_constructible<StatusOr<Immobile>>::value &&
                  !std::is_copy_assignable<StatusOr<Immobile>>::value &&
                  !std::is_move_assignable<StatusOr<Immobile>>::value,
              "immobile T requires in-place construction");
static_assert(!std::is_copy_assignable<StatusOr<AssignOnly>>::value &&
                  !std::is_move_assignable<StatusOr<AssignOnly>>::value,
              "error-to-value assignment also requires construction");
static_assert(!std::is_move_constructible<CopyOnly>::value &&
                  std::is_move_constructible<StatusOr<CopyOnly>>::value &&
                  std::is_move_assignable<StatusOr<CopyOnly>>::value,
              "a deleted defaulted move permits wrapper copy fallback");
static_assert(std::is_convertible<int, StatusOr<Implicit>>::value,
              "implicit T construction stays implicit");
static_assert(std::is_constructible<StatusOr<NoDefault>, int>::value &&
                  !std::is_convertible<int, StatusOr<NoDefault>>::value,
              "explicit T construction stays explicit");
static_assert(std::is_convertible<talon::Status, StatusOr<int>>::value,
              "Status is an implicit error source");
static_assert(!std::is_constructible<StatusOr<int>, StatusLike>::value,
              "arbitrary status-like conversions are not accepted");
static_assert(
    !std::is_constructible<StatusOr<int>, volatile talon::Status&>::value &&
        !std::is_assignable<StatusOr<int>&, volatile talon::Status&>::value,
    "volatile errors cannot be copied into a Status");
static_assert(!std::is_constructible<StatusOr<long>, StatusOr<int>>::value,
              "cross-T result conversions are intentionally absent");
static_assert(
    std::is_same<decltype(std::declval<StatusOr<int>&>().value()),
                 int&>::value &&
        std::is_same<decltype(std::declval<const StatusOr<int>&>().value()),
                     const int&>::value &&
        std::is_same<decltype(std::declval<StatusOr<int>&&>().value()),
                     int&&>::value &&
        std::is_same<decltype(std::declval<const StatusOr<int>&&>().value()),
                     const int&&>::value,
    "value cvref overloads must preserve value category");
static_assert(
    std::is_same<decltype(*std::declval<StatusOr<int>&>()), int&>::value &&
        std::is_same<decltype(*std::declval<const StatusOr<int>&>()),
                     const int&>::value &&
        std::is_same<decltype(*std::declval<StatusOr<int>&&>()),
                     int&&>::value &&
        std::is_same<decltype(*std::declval<const StatusOr<int>&&>()),
                     const int&&>::value,
    "dereference cvref overloads must preserve value category");
static_assert(
    std::is_same<decltype(std::declval<const StatusOr<int>&&>().status()),
                 talon::Status>::value,
    "const temporary status must not expose a dangling reference");

struct Fault {};
struct Tracked {
  enum Operation { kNone, kConstruct, kCopy, kMove, kCopyAssign, kMoveAssign };
  static int live;
  static int born;
  static int destroyed;
  static Operation fail;
  explicit Tracked(int value) : value(value) {
    if (fail == kConstruct) throw Fault();
    ++live;
    ++born;
  }
  Tracked(const Tracked& other) : value(other.value) {
    if (fail == kCopy) throw Fault();
    ++live;
    ++born;
  }
  Tracked(Tracked&& other) noexcept(false) : value(other.value) {
    other.value = -1;
    if (fail == kMove) throw Fault();
    ++live;
    ++born;
  }
  Tracked& operator=(const Tracked& other) {
    value = other.value;
    if (fail == kCopyAssign) throw Fault();
    return *this;
  }
  Tracked& operator=(Tracked&& other) noexcept(false) {
    value = other.value;
    other.value = -1;
    if (fail == kMoveAssign) throw Fault();
    return *this;
  }
  ~Tracked() noexcept {
    --live;
    ++destroyed;
    CHECK(live >= 0);
  }
  int value;
};
int Tracked::live = 0;
int Tracked::born = 0;
int Tracked::destroyed = 0;
Tracked::Operation Tracked::fail = Tracked::kNone;

static_assert(!std::is_nothrow_move_constructible<StatusOr<Tracked>>::value &&
                  !std::is_nothrow_move_assignable<StatusOr<Tracked>>::value,
              "throwing moves must not be declared noexcept");

#if TALON_STATUS_HAS_STD_EXPECTED
struct GreedyValue {
  GreedyValue() : converted(false) {}
  GreedyValue(const GreedyValue&) = default;
  GreedyValue(GreedyValue&&) = default;
  GreedyValue& operator=(const GreedyValue&) = default;
  GreedyValue& operator=(GreedyValue&&) = default;
  template <typename U>
  explicit GreedyValue(U&&) : converted(true) {}
  bool converted;
};

// Regression: direct native-result assignment must not bypass CheckError via
// std::expected's unexpected/copy assignment overloads when T accepts anything.
static_assert(!std::is_constructible<StatusOr<GreedyValue>,
                                     std::unexpected<talon::Status>>::value,
              "native error wrappers are not implicit value sources");
static_assert(!std::is_assignable<StatusOr<GreedyValue>&,
                                  std::unexpected<talon::Status>>::value,
              "native errors must not create a failed result with OK status");
static_assert(!std::is_constructible<StatusOr<GreedyValue>,
                                     std::expected<int, talon::Status>>::value,
              "native expected is not an implicit value source");
static_assert(!std::is_assignable<StatusOr<GreedyValue>&,
                                  std::expected<int, talon::Status>>::value,
              "native expected assignment must not replace wrapper state");

void TestNativeResultAsExplicitValue() {
  std::unexpected<talon::Status> error(talon::OkStatus());
  StatusOr<GreedyValue> result(talon::in_place, error);
  CHECK(result.ok() && result->converted);
  result = GreedyValue(error);
  CHECK(result.ok() && result->converted);
  std::expected<int, talon::Status> native(std::unexpect, talon::OkStatus());
  result = GreedyValue(native);
  CHECK(result.ok() && result->converted);
  StatusOr<std::expected<int, talon::Status>> nested(talon::in_place, 7);
  CHECK(nested.ok() && nested->has_value());
  CHECK_EQ(nested->value(), 7);
}

template <typename T>
void CompareStandardTraits() {
  typedef StatusOr<T> Result;
  typedef std::expected<T, talon::Status> Expected;
  static_assert(std::is_copy_constructible<Result>::value ==
                    std::is_copy_constructible<Expected>::value,
                "copy ctor parity");
  static_assert(std::is_move_constructible<Result>::value ==
                    std::is_move_constructible<Expected>::value,
                "move ctor parity");
  static_assert(std::is_copy_assignable<Result>::value ==
                    std::is_copy_assignable<Expected>::value,
                "copy assign parity");
  static_assert(std::is_move_assignable<Result>::value ==
                    std::is_move_assignable<Expected>::value,
                "move assign parity");
  static_assert(std::is_nothrow_move_constructible<Result>::value ==
                    std::is_nothrow_move_constructible<Expected>::value,
                "move noexcept parity");
  static_assert(std::is_nothrow_move_assignable<Result>::value ==
                    std::is_nothrow_move_assignable<Expected>::value,
                "assign noexcept parity");
}
#endif

void TestBasicsAndAccess() {
  StatusOr<NoDefault> empty;
  CHECK(!empty.ok());
  CHECK_EQ(empty.status(), talon::UnknownError());
  talon_test::CheckThrows<std::invalid_argument>(
      [] { StatusOr<int> invalid(talon::OkStatus()); });
  const talon::Status error = talon::NotFoundError("details");
  StatusOr<NoDefault> bad(error);
  CHECK_EQ(bad.status(), error);
  talon_test::CheckThrows<talon::BadStatusOrAccess>([&] { (void)bad.value(); });
  talon_test::CheckThrows<talon::BadStatusOrAccess>([&] { (void)*bad; });
  talon_test::CheckThrows<talon::BadStatusOrAccess>([&] { (void)bad->number; });
  const StatusOr<NoDefault>& const_bad = bad;
  talon_test::CheckThrows<talon::BadStatusOrAccess>(
      [&] { (void)const_bad.value(); });
  talon_test::CheckThrows<talon::BadStatusOrAccess>([&] { (void)*const_bad; });
  talon_test::CheckThrows<talon::BadStatusOrAccess>(
      [&] { (void)const_bad->number; });
  talon_test::CheckThrows<talon::BadStatusOrAccess>(
      [&] { (void)std::move(bad).value(); });
  talon_test::CheckThrows<talon::BadStatusOrAccess>(
      [&] { (void)*std::move(bad); });
  talon_test::CheckThrows<talon::BadStatusOrAccess>(
      [&] { (void)std::move(const_bad).value(); });
  talon_test::CheckThrows<talon::BadStatusOrAccess>(
      [&] { (void)*std::move(const_bad); });
  try {
    (void)bad.value();
  } catch (const talon::BadStatusOrAccess& exception) {
    CHECK_EQ(exception.status(), error);
    CHECK_EQ(std::string(exception.what()), error.ToString());
  }
  StatusOr<int> success = 42;
  CHECK(success.status().ok());
  CHECK(std::move(success).status().ok());
  talon_test::CheckThrows<std::invalid_argument>(
      [&] { success = talon::OkStatus(); });
  CHECK(success.ok() && *success == 42);
  talon_test::CheckThrows<std::invalid_argument>(
      [&] { bad = talon::OkStatus(); });
  CHECK_EQ(bad.status(), error);
  StatusOr<int*> pointer = nullptr;
  CHECK(pointer.ok());
  CHECK_EQ(*pointer, nullptr);
  StatusOr<std::unique_ptr<int>> null_owner(std::unique_ptr<int>{});
  CHECK(null_owner.ok() && !*null_owner);
  StatusOr<std::unique_ptr<int>> owner(std::unique_ptr<int>(new int(9)));
  const int* address = owner->get();
  StatusOr<std::unique_ptr<int>> moved(std::move(owner));
  CHECK(owner.ok() && !*owner);
  CHECK_EQ(moved->get(), address);
  auto extracted = std::move(moved).value();
  CHECK_EQ(extracted.get(), address);
  CHECK(moved.ok() && !*moved);
  StatusOr<NoDefault> no_default(talon::in_place, 7);
  CHECK_EQ(no_default->number, 7);
  const StatusOr<NoDefault>& constant = no_default;
  CHECK_EQ(constant->number, 7);
  CHECK_EQ(std::move(constant).value().number, 7);
  StatusOr<Immobile> immobile(talon::in_place, 11);
  CHECK_EQ(immobile->number, 11);
  StatusOr<Immobile> immobile_error;
  CHECK(!immobile_error.ok());
  const CopyOnly source(12);
  StatusOr<CopyOnly> copy(source);
  StatusOr<CopyOnly> copied_from_rvalue(std::move(copy));
  copy = std::move(copied_from_rvalue);
  CHECK_EQ(copy->value, 12);
  StatusOr<OverAligned> aligned(talon::in_place, 13);
  CHECK_EQ(reinterpret_cast<std::uintptr_t>(aligned.operator->()) %
               alignof(OverAligned),
           0U);
  CHECK_EQ(aligned->value, 13);
  const StatusOr<OverAligned>& const_aligned = aligned;
  CHECK(const_aligned.operator->() != nullptr);
  int integer = 1;
  StatusOr<std::reference_wrapper<int>> borrowed(std::ref(integer));
  borrowed->get() = 2;
  CHECK_EQ(integer, 2);
  const talon::Status copied_error = std::move(const_bad).status();
  CHECK_EQ(copied_error, error);
  const talon::Status moved_error = std::move(bad).status();
  CHECK_EQ(moved_error, error);
  CHECK(!bad.ok());
  CHECK_EQ(bad.status().code(), error.code());
  CHECK(bad.status().message().empty());
}

void TestStateTransitions() {
  CHECK_EQ(Tracked::live, 0);
  {
    StatusOr<Tracked> value(talon::in_place, 1);
    StatusOr<Tracked> other(talon::in_place, 2);
    StatusOr<Tracked> error(talon::NotFoundError("first"));
    StatusOr<Tracked> another_error(talon::InternalError("second"));
    CHECK_EQ(Tracked::live, 2);
    value = other;  // value -> value
    CHECK_EQ(value->value, 2);
    error = value;  // error -> value
    CHECK_EQ(error->value, 2);
    CHECK_EQ(Tracked::live, 3);
    value = another_error;  // value -> error
    CHECK_EQ(Tracked::live, 2);
    CHECK_EQ(value.status(), another_error.status());
    value =
        StatusOr<Tracked>(talon::UnavailableError("third"));  // error -> error
    CHECK_EQ(value.status(), talon::UnavailableError("third"));
    value = std::move(error);  // error -> value, moving
    CHECK(value.ok() && error.ok());
    CHECK_EQ(Tracked::live, 3);
    value = std::move(other);  // value -> value, moving
    CHECK_EQ(value->value, 2);
    other = talon::InternalError("direct");
    CHECK_EQ(Tracked::live, 2);
    value = std::move(another_error);  // value -> error, moving
    CHECK(!value.ok() && !another_error.ok());
    CHECK_EQ(Tracked::live, 1);
    other = std::move(value);  // error -> error, moving
    CHECK_EQ(other.status().code(), talon::StatusCode::kInternal);
    Tracked direct(8);
    other = direct;  // direct value assignment from error
    CHECK_EQ(other->value, 8);
    other = Tracked(9);
    CHECK_EQ(other->value, 9);
    StatusOr<Tracked>* same = &other;
    other = *same;
    CHECK(other.ok());
    other = std::move(*same);
    CHECK(other.ok());  // T defines its own moved-from value for self-move.
    StatusOr<Tracked>* same_error = &value;
    value = *same_error;
    value = std::move(*same_error);
    CHECK(!value.ok());
  }
  CHECK_EQ(Tracked::live, 0);
  CHECK_EQ(Tracked::born, Tracked::destroyed);
}

void TestReconstructedSubobjects() {
  StatusOr<ConstMember> constant(talon::in_place, 1);
  int first = 1;
  int second = 2;
  StatusOr<ReferenceMember> reference(talon::in_place, first);
  for (int i = 0; i < 20; ++i) {
    constant = talon::InternalError("reconstruct");
    constant = ConstMember(i);
    CHECK_EQ(constant->value, i);
    reference = talon::InternalError("rebind");
    int& expected = i % 2 == 0 ? second : first;
    reference = ReferenceMember(expected);
    CHECK_EQ(&reference->value, &expected);
  }
}

void TestThrowingLifetimes() {
  CHECK_EQ(Tracked::live, 0);
  Tracked::fail = Tracked::kConstruct;
  talon_test::CheckThrows<Fault>(
      [] { StatusOr<Tracked> v(talon::in_place, 1); });
  CHECK_EQ(Tracked::live, 0);
  Tracked::fail = Tracked::kNone;
  {
    StatusOr<Tracked> source(talon::in_place, 7);
    StatusOr<Tracked> destination(talon::NotFoundError("preserve"));
    const talon::Status before = destination.status();
    Tracked::fail = Tracked::kCopy;
    talon_test::CheckThrows<Fault>([&] { StatusOr<Tracked> copy(source); });
    CHECK_EQ(Tracked::live, 1);
    talon_test::CheckThrows<Fault>([&] { destination = source; });
    CHECK_EQ(destination.status(), before);
    CHECK_EQ(Tracked::live, 1);
    talon_test::CheckThrows<Fault>([&] { destination = *source; });
    CHECK_EQ(destination.status(), before);
    Tracked::fail = Tracked::kMove;
    talon_test::CheckThrows<Fault>(
        [&] { StatusOr<Tracked> moved(std::move(source)); });
    CHECK(source.ok());
    CHECK_EQ(Tracked::live, 1);
    talon_test::CheckThrows<Fault>([&] { destination = std::move(source); });
    CHECK_EQ(destination.status(), before);
    CHECK_EQ(Tracked::live, 1);
    talon_test::CheckThrows<Fault>([&] { destination = std::move(*source); });
    CHECK_EQ(destination.status(), before);
    Tracked::fail = Tracked::kNone;
    destination = source;
    CHECK_EQ(Tracked::live, 2);
    source->value = 91;
    Tracked::fail = Tracked::kCopyAssign;
    talon_test::CheckThrows<Fault>([&] { destination = source; });
    CHECK(destination.ok());
    CHECK_EQ(destination->value,
             91);  // T modified before throwing: basic guarantee.
    Tracked::fail = Tracked::kMoveAssign;
    talon_test::CheckThrows<Fault>([&] { destination = std::move(source); });
    CHECK(destination.ok() && source.ok());
    CHECK_EQ(Tracked::live, 2);
    Tracked::fail = Tracked::kNone;
    destination = talon::InternalError("recovered");
    CHECK_EQ(Tracked::live, 1);
    destination = Tracked(5);
    CHECK_EQ(destination->value, 5);
  }
  CHECK_EQ(Tracked::live, 0);
  CHECK_EQ(Tracked::born, Tracked::destroyed);
}

}  // namespace

int main() {
#if TALON_STATUS_HAS_STD_EXPECTED
  TestNativeResultAsExplicitValue();
  CompareStandardTraits<int>();
  CompareStandardTraits<std::unique_ptr<int>>();
  CompareStandardTraits<CopyOnly>();
  CompareStandardTraits<CopyConstructOnly>();
  CompareStandardTraits<MoveConstructOnly>();
  CompareStandardTraits<Immobile>();
  CompareStandardTraits<AssignOnly>();
  CompareStandardTraits<Tracked>();
#endif
  TestBasicsAndAccess();
  TestStateTransitions();
  TestReconstructedSubobjects();
  TestThrowingLifetimes();
  return 0;
}
