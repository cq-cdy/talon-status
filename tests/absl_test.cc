// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/strings/cord.h"
#include "talon/absl_adapter.h"
#include "talon/status_macros_short.h"
#include "test.h"

namespace {

const char kPayloadUrl[] = "type.example.test/talon-status";

std::string ErrorMessage() {
  // Exercise owned heap storage as well as embedded NUL bytes during returns.
  return std::string(1024, 'm') + std::string("bad\0input", 9);
}

talon::Status TalonError() {
  return talon::Status(talon::StatusCode::kInvalidArgument, ErrorMessage());
}

absl::Status AbslError() {
  absl::Status error(absl::StatusCode::kInvalidArgument,
                     absl::string_view(ErrorMessage()));
  error.SetPayload(kPayloadUrl, absl::Cord("owned payload"));
  return error;
}

const talon::Status& StatusOf(const talon::Status& status) { return status; }
const absl::Status& StatusOf(const absl::Status& status) { return status; }
template <typename T>
const talon::Status& StatusOf(const talon::StatusOr<T>& result) {
  return result.status();
}
template <typename T>
const absl::Status& StatusOf(const absl::StatusOr<T>& result) {
  return result.status();
}

talon::Status CanonicalStatus(const talon::Status& status) { return status; }
talon::Status CanonicalStatus(const absl::Status& status) {
  return talon::ToTalonStatus(status);
}

bool HasPayload(const talon::Status&) { return false; }
bool HasPayload(const absl::Status& status) {
  const auto payload = status.GetPayload(kPayloadUrl);
  if (payload) CHECK(*payload == absl::Cord("owned payload"));
  return payload.has_value();
}

template <typename T>
struct Sample;

template <>
struct Sample<talon::Status> {
  static talon::Status Good() { return talon::OkStatus(); }
  static talon::Status Bad() { return TalonError(); }
  static const bool is_absl = false;
};

template <>
struct Sample<absl::Status> {
  static absl::Status Good() { return absl::OkStatus(); }
  static absl::Status Bad() { return AbslError(); }
  static const bool is_absl = true;
};

template <>
struct Sample<talon::StatusOr<int>> {
  static talon::StatusOr<int> Good() { return 29; }
  static talon::StatusOr<int> Bad() { return TalonError(); }
  static const bool is_absl = false;
};

template <>
struct Sample<absl::StatusOr<int>> {
  static absl::StatusOr<int> Good() { return 29; }
  static absl::StatusOr<int> Bad() { return AbslError(); }
  static const bool is_absl = true;
};

template <>
struct Sample<talon::StatusOr<std::unique_ptr<int>>> {
  static talon::StatusOr<std::unique_ptr<int>> Good() {
    return std::unique_ptr<int>(new int(29));
  }
  static talon::StatusOr<std::unique_ptr<int>> Bad() { return TalonError(); }
  static const bool is_absl = false;
};

template <>
struct Sample<absl::StatusOr<std::unique_ptr<int>>> {
  static absl::StatusOr<std::unique_ptr<int>> Good() {
    return std::unique_ptr<int>(new int(29));
  }
  static absl::StatusOr<std::unique_ptr<int>> Bad() { return AbslError(); }
  static const bool is_absl = true;
};

template <typename Output, typename Input>
Output ReturnAcross(Input&& input, int* calls, int* after) {
  RETURN_IF_ERROR((++*calls, std::forward<Input>(input)));
  ++*after;
  return Sample<Output>::Good();
}

template <typename Input, typename Output>
void CheckOutcome(const Output& output, bool successful) {
  CHECK_EQ(output.ok(), successful);
  if (!successful) {
    CHECK_EQ(CanonicalStatus(StatusOf(output)), TalonError());
    CHECK_EQ(HasPayload(StatusOf(output)),
             Sample<Input>::is_absl && Sample<Output>::is_absl);
  }
}

template <typename Input, typename Output>
void TestReturnCombination() {
  for (int success = 0; success != 2; ++success) {
    Input input = success ? Sample<Input>::Good() : Sample<Input>::Bad();
    int calls = 0;
    int after = 0;
    const Output lvalue = ReturnAcross<Output>(input, &calls, &after);
    CheckOutcome<Input>(lvalue, success != 0);
    CHECK_EQ(calls, 1);
    CHECK_EQ(after, success);
    CheckOutcome<Input>(input, success != 0);

    calls = 0;
    after = 0;
    const Input& const_input = input;
    const Output const_lvalue =
        ReturnAcross<Output>(const_input, &calls, &after);
    CheckOutcome<Input>(const_lvalue, success != 0);
    CHECK_EQ(calls, 1);
    CHECK_EQ(after, success);

    calls = 0;
    after = 0;
    const Output rvalue =
        ReturnAcross<Output>(std::move(input), &calls, &after);
    CheckOutcome<Input>(rvalue, success != 0);
    CHECK_EQ(calls, 1);
    CHECK_EQ(after, success);
  }
}

template <typename Input>
void TestReturnTargets() {
  TestReturnCombination<Input, talon::Status>();
  TestReturnCombination<Input, absl::Status>();
  TestReturnCombination<Input, talon::StatusOr<int>>();
  TestReturnCombination<Input, absl::StatusOr<int>>();
}

template <typename Output, typename Input>
Output AssignAcross(Input input, int* calls, int* lhs_calls, int* after) {
  int values[] = {0};
  ASSIGN_OR_RETURN(values[(*lhs_calls)++], (++*calls, std::move(input)));
  CHECK_EQ(values[0], 29);
  ++*after;
  return Sample<Output>::Good();
}

template <typename Input, typename Output>
void TestAssignCombination() {
  for (int success = 0; success != 2; ++success) {
    int calls = 0;
    int lhs_calls = 0;
    int after = 0;
    const Output output = AssignAcross<Output>(
        success ? Sample<Input>::Good() : Sample<Input>::Bad(), &calls,
        &lhs_calls, &after);
    CheckOutcome<Input>(output, success != 0);
    CHECK_EQ(calls, 1);
    CHECK_EQ(lhs_calls, success);
    CHECK_EQ(after, success);
  }
}

template <typename Input>
void TestAssignTargets() {
  TestAssignCombination<Input, talon::Status>();
  TestAssignCombination<Input, absl::Status>();
  TestAssignCombination<Input, talon::StatusOr<int>>();
  TestAssignCombination<Input, absl::StatusOr<int>>();
}

template <typename Input>
void TestContinueSource() {
  int calls = 0;
  int after = 0;
  for (int i = 0; i != 4; ++i) {
    CONTINUE_IF_ERROR(
        (++calls, i % 2 == 0 ? Sample<Input>::Good() : Sample<Input>::Bad()));
    ++after;
  }
  CHECK_EQ(calls, 4);
  CHECK_EQ(after, 2);
}

void TestMacroMatrix() {
  TestReturnTargets<talon::Status>();
  TestReturnTargets<absl::Status>();
  TestReturnTargets<talon::StatusOr<int>>();
  TestReturnTargets<absl::StatusOr<int>>();
  TestAssignTargets<talon::StatusOr<int>>();
  TestAssignTargets<absl::StatusOr<int>>();
  TestContinueSource<talon::Status>();
  TestContinueSource<absl::Status>();
  TestContinueSource<talon::StatusOr<int>>();
  TestContinueSource<absl::StatusOr<int>>();
}

struct TemporaryCounts {
  int calls = 0;
  int accesses = 0;
  int after = 0;
  int owners_created = 0;
  int owners_destroyed = 0;
};

void CheckTemporaryValue(int value) { CHECK_EQ(value, 29); }
void CheckTemporaryValue(const std::unique_ptr<int>& value) {
  CHECK(value != nullptr);
  CHECK_EQ(*value, 29);
}
void CheckRetainedInput(const talon::Status&) {}
void CheckRetainedInput(const absl::Status&) {}
template <typename T>
void CheckRetainedInput(const talon::StatusOr<T>& input) {
  if (input.ok()) CheckTemporaryValue(input.value());
}
template <typename T>
void CheckRetainedInput(const absl::StatusOr<T>& input) {
  if (input.ok()) CheckTemporaryValue(input.value());
}

template <typename Input>
class TemporaryOwner {
 public:
  TemporaryOwner(Input input, TemporaryCounts& counts)
      : input_(std::move(input)), counts_(counts) {
    ++counts_.owners_created;
  }
  ~TemporaryOwner() {
    CheckRetainedInput(input_);
    ++counts_.owners_destroyed;
  }
  const Input& Get() const {
    ++counts_.accesses;
    return input_;
  }
  TemporaryOwner(const TemporaryOwner&) = delete;
  TemporaryOwner(TemporaryOwner&&) = delete;

 private:
  Input input_;
  TemporaryCounts& counts_;
};

#if defined(__clang__)
#pragma clang diagnostic push
// This intentional use checks a temporary whose lifetime is not extended by
// std::move, independently of the compiler's performance recommendation.
#pragma clang diagnostic ignored "-Wpessimizing-move"
#endif
template <typename Output, typename Input>
Output ReturnMovedTemporary(bool success, TemporaryCounts& counts) {
  RETURN_IF_ERROR(std::move((
      ++counts.calls, success ? Sample<Input>::Good() : Sample<Input>::Bad())));
  ++counts.after;
  return Sample<Output>::Good();
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

template <typename Output, typename Input>
Output ReturnFromTemporaryOwner(bool success, TemporaryCounts& counts) {
  RETURN_IF_ERROR(
      TemporaryOwner<Input>((++counts.calls, success ? Sample<Input>::Good()
                                                     : Sample<Input>::Bad()),
                            counts)
          .Get());
  ++counts.after;
  return Sample<Output>::Good();
}

template <typename Input, typename Output>
void TestTemporaryReturnCombination() {
  for (int success = 0; success != 2; ++success) {
    for (int owner = 0; owner != 2; ++owner) {
      TemporaryCounts counts;
      const Output output =
          owner ? ReturnFromTemporaryOwner<Output, Input>(success != 0, counts)
                : ReturnMovedTemporary<Output, Input>(success != 0, counts);
      // Includes full message bytes and same-system Abseil payload retention.
      CheckOutcome<Input>(output, success != 0);
      CHECK_EQ(counts.calls, 1);
      CHECK_EQ(counts.accesses, owner);
      CHECK_EQ(counts.after, success);
      CHECK_EQ(counts.owners_created, owner);
      CHECK_EQ(counts.owners_destroyed, owner);
    }
  }
}

template <typename Input>
void TestTemporaryReturnTargets() {
  TestTemporaryReturnCombination<Input, talon::Status>();
  TestTemporaryReturnCombination<Input, absl::Status>();
  TestTemporaryReturnCombination<Input, talon::StatusOr<int>>();
  TestTemporaryReturnCombination<Input, absl::StatusOr<int>>();
}

void TestTemporaryReturnMatrix() {
  TestTemporaryReturnTargets<talon::Status>();
  TestTemporaryReturnTargets<absl::Status>();
  TestTemporaryReturnTargets<talon::StatusOr<int>>();
  TestTemporaryReturnTargets<absl::StatusOr<int>>();
  TestTemporaryReturnTargets<talon::StatusOr<std::unique_ptr<int>>>();
  TestTemporaryReturnTargets<absl::StatusOr<std::unique_ptr<int>>>();
}

void TestCodeAndMessageMapping() {
  struct CodePair {
    talon::StatusCode talon_code;
    absl::StatusCode absl_code;
  };
  const CodePair codes[] = {
      {talon::StatusCode::kOk, absl::StatusCode::kOk},
      {talon::StatusCode::kCancelled, absl::StatusCode::kCancelled},
      {talon::StatusCode::kUnknown, absl::StatusCode::kUnknown},
      {talon::StatusCode::kInvalidArgument, absl::StatusCode::kInvalidArgument},
      {talon::StatusCode::kDeadlineExceeded,
       absl::StatusCode::kDeadlineExceeded},
      {talon::StatusCode::kNotFound, absl::StatusCode::kNotFound},
      {talon::StatusCode::kAlreadyExists, absl::StatusCode::kAlreadyExists},
      {talon::StatusCode::kPermissionDenied,
       absl::StatusCode::kPermissionDenied},
      {talon::StatusCode::kResourceExhausted,
       absl::StatusCode::kResourceExhausted},
      {talon::StatusCode::kFailedPrecondition,
       absl::StatusCode::kFailedPrecondition},
      {talon::StatusCode::kAborted, absl::StatusCode::kAborted},
      {talon::StatusCode::kOutOfRange, absl::StatusCode::kOutOfRange},
      {talon::StatusCode::kUnimplemented, absl::StatusCode::kUnimplemented},
      {talon::StatusCode::kInternal, absl::StatusCode::kInternal},
      {talon::StatusCode::kUnavailable, absl::StatusCode::kUnavailable},
      {talon::StatusCode::kDataLoss, absl::StatusCode::kDataLoss},
      {talon::StatusCode::kUnauthenticated,
       absl::StatusCode::kUnauthenticated}};
  static_assert(sizeof(codes) / sizeof(codes[0]) == 17,
                "every canonical status code must be checked");
  const std::string message = ErrorMessage();
  for (const CodePair& code : codes) {
    const absl::Status source(code.absl_code, absl::string_view(message));
    const talon::Status converted = talon::ToTalonStatus(source);
    CHECK_EQ(converted.code(), code.talon_code);
    CHECK_EQ(converted.message(), source.ok() ? std::string() : message);
    const absl::Status round_trip = talon::ToAbslStatus(converted);
    CHECK_EQ(round_trip.code(), code.absl_code);
    CHECK_EQ(round_trip.message(), source.message());

    const talon::Status talon_source(code.talon_code, message);
    const absl::Status absl_converted = talon::ToAbslStatus(talon_source);
    CHECK_EQ(absl_converted.code(), code.absl_code);
    CHECK_EQ(talon::ToTalonStatus(absl_converted), talon_source);

    const absl::Status empty_message(code.absl_code, "");
    const talon::Status empty_converted = talon::ToTalonStatus(empty_message);
    CHECK_EQ(empty_converted.code(), code.talon_code);
    CHECK(empty_converted.message().empty());
  }

  const absl::Status unknown(static_cast<absl::StatusCode>(999), "future code");
  const talon::Status converted_unknown = talon::ToTalonStatus(unknown);
  CHECK_EQ(converted_unknown.code(), talon::StatusCode::kUnknown);
  CHECK_EQ(converted_unknown.message(), "future code");

  const absl::Status payload_source = AbslError();
  CHECK(HasPayload(payload_source));
  const talon::Status no_payload = talon::ToTalonStatus(payload_source);
  CHECK_EQ(no_payload, TalonError());
  CHECK(!HasPayload(talon::ToAbslStatus(no_payload)));

  talon::Status owned;
  {
    std::string temporary = "temporary message";
    owned = talon::ToTalonStatus(absl::InternalError(temporary));
    temporary.assign("modified");
  }
  CHECK_EQ(owned.message(), "temporary message");
}

void TestNamedResultConversions() {
  const absl::StatusOr<std::string> absl_value(std::string("owned"));
  const talon::StatusOr<std::string> talon_copy =
      talon::ToTalonStatusOr(absl_value);
  CHECK_EQ(*talon_copy, "owned");
  CHECK_EQ(*absl_value, "owned");
  const absl::StatusOr<std::string> absl_copy =
      talon::ToAbslStatusOr(talon_copy);
  CHECK_EQ(*absl_copy, "owned");

  const absl::StatusOr<int> absl_error = AbslError();
  const talon::StatusOr<int> talon_error = talon::ToTalonStatusOr(absl_error);
  CHECK_EQ(talon_error.status(), TalonError());
  CHECK_EQ(CanonicalStatus(talon::ToAbslStatusOr(talon_error).status()),
           TalonError());
  CHECK(!HasPayload(talon::ToAbslStatusOr(talon_error).status()));

  absl::StatusOr<std::unique_ptr<int>> absl_owner(
      std::unique_ptr<int>(new int(73)));
  talon::StatusOr<std::unique_ptr<int>> talon_owner =
      talon::ToTalonStatusOr(std::move(absl_owner));
  CHECK_EQ(**talon_owner, 73);
  CHECK(absl_owner.ok());
  CHECK(!absl_owner.value());
  absl::StatusOr<std::unique_ptr<int>> returned_owner =
      talon::ToAbslStatusOr(std::move(talon_owner));
  CHECK_EQ(**returned_owner, 73);
  CHECK(talon_owner.ok());
  CHECK(!talon_owner.value());

  CHECK_EQ(
      talon::ToTalonStatusOr(absl::StatusOr<std::unique_ptr<int>>(AbslError()))
          .status(),
      TalonError());
  CHECK_EQ(
      CanonicalStatus(talon::ToAbslStatusOr(
                          talon::StatusOr<std::unique_ptr<int>>(TalonError()))
                          .status()),
      TalonError());
}

template <typename Output, typename Input>
Output MoveAcross(Input input, int* calls, int* after) {
  ASSIGN_OR_RETURN(std::unique_ptr<int> value, (++*calls, std::move(input)));
  CHECK_EQ(*value, 83);
  ++*after;
  return value;
}

template <typename Input, typename Output>
void TestMoveCombination() {
  typedef
      typename std::decay<decltype(std::declval<const Input&>().status())>::type
          InputStatus;
  typedef typename std::decay<
      decltype(std::declval<const Output&>().status())>::type OutputStatus;
  for (int success = 0; success != 2; ++success) {
    int calls = 0;
    int after = 0;
    Input input = success ? Input(std::unique_ptr<int>(new int(83)))
                          : Input(Sample<InputStatus>::Bad());
    const Output output = MoveAcross<Output>(std::move(input), &calls, &after);
    CHECK_EQ(output.ok(), success != 0);
    if (success) {
      CHECK_EQ(**output, 83);
    } else {
      CHECK_EQ(CanonicalStatus(output.status()), TalonError());
      CHECK_EQ(HasPayload(output.status()),
               Sample<InputStatus>::is_absl && Sample<OutputStatus>::is_absl);
    }
    CHECK_EQ(calls, 1);
    CHECK_EQ(after, success);
  }
}

template <typename Input>
void TestMoveErrorTargets(Input input) {
  int calls = 0;
  int after = 0;
  const talon::Status status =
      ReturnAcross<talon::Status>(input, &calls, &after);
  CHECK_EQ(status, TalonError());
  const absl::Status absl_status =
      ReturnAcross<absl::Status>(input, &calls, &after);
  CHECK_EQ(CanonicalStatus(absl_status), TalonError());
  const talon::StatusOr<int> talon_result =
      ReturnAcross<talon::StatusOr<int>>(input, &calls, &after);
  CHECK_EQ(talon_result.status(), TalonError());
  const absl::StatusOr<int> absl_result =
      ReturnAcross<absl::StatusOr<int>>(input, &calls, &after);
  CHECK_EQ(CanonicalStatus(absl_result.status()), TalonError());
  CHECK_EQ(calls, 4);
  CHECK_EQ(after, 0);
}

talon::Status CheckMoveOnlyLvalues() {
  absl::StatusOr<std::unique_ptr<int>> owner(std::unique_ptr<int>(new int(97)));
  RETURN_IF_ERROR(owner);
  const absl::StatusOr<std::unique_ptr<int>>& const_owner = owner;
  RETURN_IF_ERROR(const_owner);
  CHECK_EQ(**owner, 97);
  int after = 0;
  for (int i = 0; i != 2; ++i) {
    CONTINUE_IF_ERROR(owner);
    ++after;
  }
  CHECK_EQ(after, 2);
  CHECK_EQ(**owner, 97);
  return talon::OkStatus();
}

// A broadly converting value constructor must not consume an error-return
// proxy and accidentally create a successful result.
struct Greedy {
  template <typename T>
  Greedy(T&&) {}  // NOLINT
};

talon::StatusOr<Greedy> GreedyTalonReturn() {
  RETURN_IF_ERROR(AbslError());
  return Greedy(1);
}

absl::StatusOr<Greedy> GreedyAbslReturn() {
  RETURN_IF_ERROR(TalonError());
  return Greedy(1);
}

void TestMoveOnlyAndGreedyValues() {
  typedef talon::StatusOr<std::unique_ptr<int>> TalonPointer;
  typedef absl::StatusOr<std::unique_ptr<int>> AbslPointer;
  TestMoveCombination<TalonPointer, TalonPointer>();
  TestMoveCombination<TalonPointer, AbslPointer>();
  TestMoveCombination<AbslPointer, TalonPointer>();
  TestMoveCombination<AbslPointer, AbslPointer>();
  TestMoveErrorTargets(TalonPointer(TalonError()));
  TestMoveErrorTargets(AbslPointer(AbslError()));
  CHECK(CheckMoveOnlyLvalues().ok());
  CHECK_EQ(GreedyTalonReturn().status(), TalonError());
  CHECK_EQ(CanonicalStatus(GreedyAbslReturn().status()), TalonError());
}

}  // namespace

int main() {
  TestCodeAndMessageMapping();
  TestNamedResultConversions();
  TestMacroMatrix();
  TestTemporaryReturnMatrix();
  TestMoveOnlyAndGreedyValues();
  return 0;
}
