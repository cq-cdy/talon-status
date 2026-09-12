// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "talon/status_macros.h"
#include "talon/status_macros_short.h"
#include "test.h"

namespace {

talon::Status CheckedStatus(bool success, int* calls) {
  ++*calls;
  return success ? talon::OkStatus() : talon::NotFoundError("missing");
}

talon::StatusOr<int> CheckedValue(bool success, int* calls) {
  ++*calls;
  if (!success) return talon::NotFoundError("missing");
  return 17;
}

talon::Status AssignExisting(bool success, int* calls, int* lhs_calls,
                             int* after) {
  int values[] = {3};
  ASSIGN_OR_RETURN(values[(*lhs_calls)++], CheckedValue(success, calls));
  CHECK_EQ(values[0], 17);
  ++*after;
  return talon::OkStatus();
}

talon::Status ReturnStatus(bool success, int* calls, int* after) {
  RETURN_IF_ERROR(CheckedStatus(success, calls));
  ++*after;
  return talon::OkStatus();
}

talon::Status ReturnResult(bool success, int* calls, int* after) {
  RETURN_IF_ERROR(CheckedValue(success, calls));
  ++*after;
  return talon::OkStatus();
}

void TestEvaluationAndReturn() {
  for (int successful = 0; successful != 2; ++successful) {
    int calls = 0;
    int lhs_calls = 0;
    int after = 0;
    const talon::Status assigned =
        AssignExisting(successful != 0, &calls, &lhs_calls, &after);
    CHECK_EQ(assigned.ok(), successful != 0);
    CHECK_EQ(calls, 1);
    CHECK_EQ(lhs_calls, successful);
    CHECK_EQ(after, successful);
    if (!successful) CHECK_EQ(assigned, talon::NotFoundError("missing"));

    calls = 0;
    after = 0;
    const talon::Status status = ReturnStatus(successful != 0, &calls, &after);
    CHECK_EQ(status.ok(), successful != 0);
    CHECK_EQ(calls, 1);
    CHECK_EQ(after, successful);

    calls = 0;
    after = 0;
    const talon::Status result = ReturnResult(successful != 0, &calls, &after);
    CHECK_EQ(result.ok(), successful != 0);
    CHECK_EQ(calls, 1);
    CHECK_EQ(after, successful);
  }
}

struct Immovable {
  explicit Immovable(int n) : number(n) {}
  Immovable(const Immovable&) = delete;
  Immovable(Immovable&&) = delete;
  int number;
};

talon::Status CheckBorrowedResults() {
  talon::StatusOr<std::unique_ptr<int>> owner(
      std::unique_ptr<int>(new int(23)));
  RETURN_IF_ERROR(owner);
  CHECK_EQ(**owner, 23);
  const talon::StatusOr<std::unique_ptr<int>>& const_owner = owner;
  RETURN_IF_ERROR(const_owner);
  CHECK_EQ(**owner, 23);
  // Neither checking an rvalue result nor a const lvalue reads/moves its value.
  RETURN_IF_ERROR(std::move(owner));
  CHECK_EQ(**owner, 23);
  talon::StatusOr<Immovable> immovable(talon::in_place, 31);
  RETURN_IF_ERROR(immovable);
  CHECK_EQ(immovable->number, 31);
  const talon::Status error = talon::DataLossError("const status");
  RETURN_IF_ERROR(error);
  CHECK(false);
  return talon::OkStatus();
}

template <typename T>
talon::StatusOr<T> Extract(talon::StatusOr<T> source) {
  ASSIGN_OR_RETURN(T result, std::move(source));
  RETURN_IF_ERROR(talon::OkStatus());
  return result;
}

template <typename A, typename B>
talon::StatusOr<std::pair<A, B>> MakePair(A first, B second) {
  return std::make_pair(first, second);
}

#define TEST_WRAPPED_ASSIGN(lhs, expr) TALON_ASSIGN_OR_RETURN(lhs, expr)
#define TEST_WRAPPED_VALUE(calls) CheckedValue(true, calls)
#define TEST_TWO_ASSIGNMENTS(calls)                               \
  TEST_WRAPPED_ASSIGN(int nested_one, CheckedValue(true, calls)); \
  TEST_WRAPPED_ASSIGN(int nested_two, CheckedValue(true, calls))

talon::Status CheckScopeAndSyntax() {
  int calls = 0;
  ASSIGN_OR_RETURN(int declared, CheckedValue(true, &calls));
  CHECK_EQ(declared, 17);
  CHECK_EQ(calls, 1);
  TEST_WRAPPED_ASSIGN(int wrapped, TEST_WRAPPED_VALUE(&calls));
  CHECK_EQ(wrapped, 17);
  talon::StatusOr<int> source = 12;
  ASSIGN_OR_RETURN(int copied, source);
  CHECK_EQ(copied, 12);
  CHECK_EQ(*source, 12);
  const talon::StatusOr<int>& const_source = source;
  ASSIGN_OR_RETURN(int const_copied, const_source);
  CHECK_EQ(const_copied, 12);
  ASSIGN_OR_RETURN((std::pair<int, int> pair), (MakePair<int, int>(4, 9)));
  CHECK_EQ(pair.first, 4);
  CHECK_EQ(pair.second, 9);
  ASSIGN_OR_RETURN((pair), (MakePair<int, int>(5, 8)));
  CHECK_EQ(pair.first, 5);
#if TALON_STATUS_INTERNAL_LANGUAGE >= 201703L
  ASSIGN_OR_RETURN((auto [first, second]), (MakePair<int, int>(6, 7)));
  CHECK_EQ(first + second, 13);
#endif

  struct Holder {
    int field;
  } holder = {0};
  ASSIGN_OR_RETURN(((holder).field), CheckedValue(true, &calls));
  CHECK_EQ(holder.field, 17);
  int* pointer = &holder.field;
  ASSIGN_OR_RETURN(*pointer, CheckedValue(true, &calls));
  CHECK_EQ(holder.field, 17);

  int result = 10;
  int status = 20;
  int talon_status_result_0 = 30;
  ASSIGN_OR_RETURN(result, CheckedValue(true, &calls));
  CHECK_EQ(result + status + talon_status_result_0, 67);

#if TALON_STATUS_USE_LINE_COUNTER || !defined(__COUNTER__)
  ASSIGN_OR_RETURN(int same_one, CheckedValue(true, &calls));
  ASSIGN_OR_RETURN(int same_two, CheckedValue(true, &calls));
#else
  ASSIGN_OR_RETURN(int same_one, CheckedValue(true, &calls)); ASSIGN_OR_RETURN(int same_two, CheckedValue(true, &calls));
#endif
  CHECK_EQ(same_one + same_two, 34);
#if TALON_STATUS_USE_LINE_COUNTER || !defined(__COUNTER__)
  TEST_WRAPPED_ASSIGN(int nested_one, CheckedValue(true, &calls));
  TEST_WRAPPED_ASSIGN(int nested_two, CheckedValue(true, &calls));
#else
  TEST_TWO_ASSIGNMENTS(&calls);
#endif
  CHECK_EQ(nested_one + nested_two, 34);
  RETURN_IF_ERROR(talon::OkStatus()); RETURN_IF_ERROR(talon::OkStatus());
  return talon::OkStatus();
}

#undef TEST_TWO_ASSIGNMENTS
#undef TEST_WRAPPED_VALUE
#undef TEST_WRAPPED_ASSIGN

struct Tracked {
  explicit Tracked(int n) : number(n) { ++alive; }
  Tracked(const Tracked& other) : number(other.number) {
    ++alive;
    ++copies;
  }
  Tracked(Tracked&& other) noexcept : number(other.number) {
    other.number = -1;
    ++alive;
  }
  ~Tracked() { --alive; }
  int number;
  static int alive;
  static int copies;
};

int Tracked::alive = 0;
int Tracked::copies = 0;

talon::StatusOr<Tracked> MakeTracked(int number) {
  return talon::StatusOr<Tracked>(talon::in_place, number);
}

talon::Status CheckReferenceLifetime(bool early_return) {
  ASSIGN_OR_RETURN(const Tracked& first, MakeTracked(4));
  CHECK_EQ(first.number, 4);
  ASSIGN_OR_RETURN(Tracked && second, MakeTracked(5));
  CHECK_EQ(second.number, 5);
  ASSIGN_OR_RETURN(auto&& third, MakeTracked(6));
  CHECK_EQ(third.number, 6);
  CHECK_EQ(first.number + second.number + third.number, 15);
  CHECK_EQ(Tracked::alive, 3);
  if (early_return) {
    RETURN_IF_ERROR(talon::AbortedError("cleanup"));
  }
  return talon::OkStatus();
}

talon::Status CheckOwnedValueCleanup() {
  ASSIGN_OR_RETURN(Tracked value, MakeTracked(9));
  CHECK_EQ(value.number, 9);
  RETURN_IF_ERROR(talon::AbortedError("cleanup"));
  CHECK(false);
  return talon::OkStatus();
}

void TestValuesAndLifetime() {
  CHECK_EQ(CheckBorrowedResults(), talon::DataLossError("const status"));
  talon::StatusOr<std::unique_ptr<int>> result = Extract(
      talon::StatusOr<std::unique_ptr<int>>(std::unique_ptr<int>(new int(41))));
  CHECK(result.ok());
  CHECK_EQ(**result, 41);
  CHECK_EQ(
      Extract(talon::StatusOr<int>(talon::InternalError("failure"))).status(),
      talon::InternalError("failure"));
  CHECK(CheckScopeAndSyntax().ok());
  CHECK_EQ(Tracked::alive, 0);
  CHECK(CheckReferenceLifetime(false).ok());
  CHECK_EQ(Tracked::alive, 0);
  CHECK_EQ(CheckReferenceLifetime(true), talon::AbortedError("cleanup"));
  CHECK_EQ(Tracked::alive, 0);
  CHECK_EQ(CheckOwnedValueCleanup(), talon::AbortedError("cleanup"));
  CHECK_EQ(Tracked::alive, 0);
  CHECK_EQ(Tracked::copies, 0);
}

struct TemporaryCounts {
  int calls = 0;
  int accesses = 0;
  int after = 0;
  int owners_created = 0;
  int owners_destroyed = 0;
  int values_created = 0;
  int values_destroyed = 0;
};

struct TemporaryValue {
  explicit TemporaryValue(TemporaryCounts& counts) : counts(counts) {
    ++counts.values_created;
  }
  ~TemporaryValue() { ++counts.values_destroyed; }
  TemporaryValue(const TemporaryValue&) = delete;
  TemporaryValue(TemporaryValue&&) = delete;
  TemporaryCounts& counts;
};
typedef talon::StatusOr<std::unique_ptr<TemporaryValue>> TemporaryResult;

talon::Status TemporaryError() {
  return talon::DataLossError(std::string(1024, 'x'), std::string("\0tail", 5));
}

template <typename R>
struct TemporarySource;
template <>
struct TemporarySource<talon::Status> {
  static talon::Status Make(bool success, TemporaryCounts& counts) {
    ++counts.calls;
    return success ? talon::OkStatus() : TemporaryError();
  }
  static const bool owns_value = false;
};
template <>
struct TemporarySource<TemporaryResult> {
  static TemporaryResult Make(bool success, TemporaryCounts& counts) {
    ++counts.calls;
    if (!success) return TemporaryError();
    return std::unique_ptr<TemporaryValue>(new TemporaryValue(counts));
  }
  static const bool owns_value = true;
};

void CheckRetainedValue(const talon::Status&) {}
void CheckRetainedValue(const TemporaryResult& result) {
  if (result.ok()) CHECK(result.value() != nullptr);
}

template <typename R>
class TemporaryOwner {
 public:
  TemporaryOwner(R result, TemporaryCounts& counts)
      : result_(std::move(result)), counts_(counts) {
    ++counts_.owners_created;
  }
  ~TemporaryOwner() {
    // Checking a successful borrowed result must not consume its value.
    CheckRetainedValue(result_);
    ++counts_.owners_destroyed;
  }
  const R& Get() const {
    ++counts_.accesses;
    return result_;
  }
  TemporaryOwner(const TemporaryOwner&) = delete;
  TemporaryOwner(TemporaryOwner&&) = delete;

 private:
  R result_;
  TemporaryCounts& counts_;
};

#if defined(__clang__)
#pragma clang diagnostic push
// Deliberately test std::move of a temporary: the macro must inspect the
// borrowed object before the expression that owns it ends.
#pragma clang diagnostic ignored "-Wpessimizing-move"
#endif
template <typename R>
talon::Status ReturnMovedTemporary(bool success, TemporaryCounts& counts) {
  RETURN_IF_ERROR(std::move(TemporarySource<R>::Make(success, counts)));
  ++counts.after;
  return talon::OkStatus();
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

template <typename R>
talon::Status ReturnFromTemporaryOwner(bool success, TemporaryCounts& counts) {
  RETURN_IF_ERROR(
      TemporaryOwner<R>(TemporarySource<R>::Make(success, counts), counts)
          .Get());
  ++counts.after;
  return talon::OkStatus();
}

template <typename R>
const R& BorrowTemporary(const R& result, TemporaryCounts& counts) {
  ++counts.calls;
  return result;
}

talon::Status ReturnImmovableTemporary(TemporaryCounts& counts) {
  RETURN_IF_ERROR(BorrowTemporary(
      talon::StatusOr<TemporaryValue>(talon::in_place, counts), counts));
  ++counts.after;
  return talon::OkStatus();
}

template <typename R>
void TestTemporarySource() {
  for (int success = 0; success != 2; ++success) {
    for (int owner = 0; owner != 2; ++owner) {
      TemporaryCounts counts;
      const talon::Status output =
          owner ? ReturnFromTemporaryOwner<R>(success != 0, counts)
                : ReturnMovedTemporary<R>(success != 0, counts);
      CHECK_EQ(output, success ? talon::OkStatus() : TemporaryError());
      CHECK_EQ(counts.calls, 1);
      CHECK_EQ(counts.accesses, owner);
      CHECK_EQ(counts.after, success);
      CHECK_EQ(counts.owners_created, owner);
      CHECK_EQ(counts.owners_destroyed, owner);
      CHECK_EQ(counts.values_created,
               TemporarySource<R>::owns_value ? success : 0);
      CHECK_EQ(counts.values_destroyed, counts.values_created);
    }
  }
}

void TestTemporaryReferences() {
  TestTemporarySource<talon::Status>();
  TestTemporarySource<TemporaryResult>();
  TemporaryCounts counts;
  CHECK(ReturnImmovableTemporary(counts).ok());
  CHECK_EQ(counts.calls, 1);
  CHECK_EQ(counts.after, 1);
  CHECK_EQ(counts.values_created, 1);
  CHECK_EQ(counts.values_destroyed, 1);
}

talon::Status OddError(int value, int* calls) {
  return CheckedStatus(value % 2 == 0, calls);
}

void TestLoopKinds() {
  int calls = 0;
  std::vector<int> visited;
  for (int i = 0; i != 6; ++i) {
    CONTINUE_IF_ERROR(OddError(i, &calls));
    visited.push_back(i);
  }
  const std::vector<int> expected = {0, 2, 4};
  CHECK(visited == expected);
  CHECK_EQ(calls, 6);

  calls = 0;
  visited.clear();
  int current = -1;
  while (++current < 6) {
    CONTINUE_IF_ERROR(OddError(current, &calls));
    visited.push_back(current);
  }
  CHECK(visited == expected);
  CHECK_EQ(calls, 6);

  calls = 0;
  visited.clear();
  current = -1;
  do {
    ++current;
    CONTINUE_IF_ERROR(OddError(current, &calls));
    visited.push_back(current);
  } while (current < 5);
  CHECK(visited == expected);
  CHECK_EQ(calls, 6);

  calls = 0;
  visited.clear();
  const std::vector<int> items = {0, 1, 2, 3, 4, 5};
  for (const int item : items) {
    CONTINUE_IF_ERROR(OddError(item, &calls));
    visited.push_back(item);
  }
  CHECK(visited == expected);
  CHECK_EQ(calls, 6);

  int outer_after = 0;
  int sum = 0;
  calls = 0;
  for (int outer = 0; outer != 3; ++outer) {
    for (int inner = 0; inner != 4; ++inner) {
      CONTINUE_IF_ERROR(OddError(inner, &calls));
      sum += outer * 10 + inner;
    }
    ++outer_after;
  }
  CHECK_EQ(sum, 66);
  CHECK_EQ(calls, 12);
  CHECK_EQ(outer_after, 3);

  calls = 0;
  for (int i = 0; i != 4; ++i) CONTINUE_IF_ERROR(OddError(i, &calls));
  CHECK_EQ(calls, 4);

  calls = 0;
  current = 0;
  while (current++ < 4) CONTINUE_IF_ERROR(OddError(current, &calls));
  CHECK_EQ(calls, 4);
  calls = 0;
  current = 0;
  do CONTINUE_IF_ERROR(OddError(current++, &calls));
  while (current < 4);
  CHECK_EQ(calls, 4);
  calls = 0;
  for (const int item : items) CONTINUE_IF_ERROR(OddError(item, &calls));
  CHECK_EQ(calls, 6);

  calls = 0;
  int result_after = 0;
  for (int i = 0; i != 4; ++i) {
    CONTINUE_IF_ERROR(CheckedValue(i % 2 == 0, &calls));
    ++result_after;
  }
  CHECK_EQ(calls, 4);
  CHECK_EQ(result_after, 2);
}

talon::Status IfElseReturn(bool branch, bool success, int* calls,
                           int* else_calls, int* after) {
  if (branch)
    RETURN_IF_ERROR(CheckedStatus(success, calls));
  else
    ++*else_calls;
  ++*after;
  return talon::OkStatus();
}

talon::Status UnbracedLoopReturn(bool success, int* calls) {
  for (int i = 0; i != 3; ++i) RETURN_IF_ERROR(CheckedStatus(success, calls));
  return talon::OkStatus();
}

talon::StatusOr<int> SwitchReturn(int branch, bool success, int* calls) {
  switch (branch) {
    case 0:
      RETURN_IF_ERROR(CheckedStatus(success, calls));
      break;
    case 1: {
      ASSIGN_OR_RETURN(int declared, CheckedValue(success, calls));
      return declared;
    }
    default:
      break;
  }
  return 7;
}

void TestBranchSyntax() {
  for (int branch = 0; branch != 2; ++branch) {
    for (int success = 0; success != 2; ++success) {
      int calls = 0;
      int else_calls = 0;
      int after = 0;
      const talon::Status result =
          IfElseReturn(branch != 0, success != 0, &calls, &else_calls, &after);
      CHECK_EQ(result.ok(), !branch || success);
      CHECK_EQ(calls, branch);
      CHECK_EQ(else_calls, !branch);
      CHECK_EQ(after, !branch || success);
    }
  }
  int calls = 0;
  CHECK(UnbracedLoopReturn(true, &calls).ok());
  CHECK_EQ(calls, 3);
  calls = 0;
  CHECK(!UnbracedLoopReturn(false, &calls).ok());
  CHECK_EQ(calls, 1);

  calls = 0;
  int else_calls = 0;
  int after = 0;
  for (int i = 0; i != 4; ++i) {
    if (i != 2)
      CONTINUE_IF_ERROR(OddError(i, &calls));
    else
      ++else_calls;
    ++after;
  }
  CHECK_EQ(calls, 3);
  CHECK_EQ(else_calls, 1);
  CHECK_EQ(after, 2);

  int successful_cases = 0;
  int default_cases = 0;
  after = 0;
  for (int i = 0; i != 6; ++i) {
    switch (i % 3) {
      case 0:
        CONTINUE_IF_ERROR(talon::InternalError("skip current iteration"));
        CHECK(false);
        break;
      case 1:
        CONTINUE_IF_ERROR(talon::OkStatus());
        ++successful_cases;
        break;
      default:
        ++default_cases;
        break;
    }
    ++after;
  }
  CHECK_EQ(successful_cases, 2);
  CHECK_EQ(default_cases, 2);
  CHECK_EQ(after, 4);

  calls = 0;
  CHECK_EQ(*SwitchReturn(0, true, &calls), 7);
  CHECK_EQ(*SwitchReturn(1, true, &calls), 17);
  CHECK(!SwitchReturn(0, false, &calls).ok());
  CHECK(!SwitchReturn(1, false, &calls).ok());
  CHECK_EQ(calls, 4);

  const auto lambda = [](bool success) -> talon::StatusOr<int> {
    int evaluations = 0;
    ASSIGN_OR_RETURN(int value, CheckedValue(success, &evaluations));
    RETURN_IF_ERROR(talon::OkStatus());
    CHECK_EQ(evaluations, 1);
    return value;
  };
  CHECK_EQ(*lambda(true), 17);
  CHECK_EQ(lambda(false).status(), talon::NotFoundError("missing"));
}

}  // namespace

int main() {
  TestEvaluationAndReturn();
  TestValuesAndLifetime();
  TestTemporaryReferences();
  TestLoopKinds();
  TestBranchSyntax();
  return 0;
}
