// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/cord.h"
#include "talon/absl_adapter.h"
#include "talon/status_macros_short.h"
#include "test.h"

namespace {

enum Phase { kSuccess, kEntryError, kExitError };
const char kPayloadUrl[] = "type.example.test/talon-long-chain";

std::string Message(int level, Phase phase) {
  std::string message = "adapter/level=" + std::to_string(level) +
                        "/phase=" + std::to_string(static_cast<int>(phase));
  message.push_back('\0');
  message += std::string(1024, static_cast<char>('A' + level % 26));
  message += "/end";
  return message;
}

struct Context {
  Context(int depth, Phase phase, int failure)
      : depth(depth),
        phase(phase),
        failure(failure),
        code(static_cast<talon::StatusCode>(1 + failure % 16)),
        message(Message(failure, phase)) {}
  int depth;
  Phase phase;
  int failure;
  talon::StatusCode code;
  std::string message;
  int entries = 0;
  int exits = 0;
  int child_calls = 0;
  int completed = 0;
  int frames_created = 0;
  int frames_destroyed = 0;
  int frames_live = 0;
  int maximum_frames = 0;
  int values_created = 0;
  int values_destroyed = 0;
  int values_live = 0;
  int stamps_created = 0;
  int stamps_destroyed = 0;
  int stamps_live = 0;
  int audit_calls = 0;
  int item_calls = 0;
  int item_successes = 0;
  int assignments = 0;
};

class Frame {
 public:
  explicit Frame(Context& context) : context_(context) {
    ++context_.frames_created;
    ++context_.frames_live;
    context_.maximum_frames =
        std::max(context_.maximum_frames, context_.frames_live);
  }
  ~Frame() {
    --context_.frames_live;
    ++context_.frames_destroyed;
  }
  Frame(const Frame&) = delete;
  Frame& operator=(const Frame&) = delete;

 private:
  Context& context_;
};

struct Value {
  explicit Value(Context& context) : context(context), score(7) {
    ++context.values_created;
    ++context.values_live;
  }
  ~Value() {
    ++context.values_destroyed;
    --context.values_live;
  }
  Value(const Value&) = delete;
  Value& operator=(const Value&) = delete;
  Context& context;
  std::uint64_t score;
};
typedef std::unique_ptr<Value> Pointer;
typedef talon::StatusOr<Pointer> TalonResult;
typedef absl::StatusOr<Pointer> AbslResult;

struct Stamp {
  explicit Stamp(Context& context) : context(context) {
    ++context.stamps_created;
    ++context.stamps_live;
  }
  ~Stamp() {
    ++context.stamps_destroyed;
    --context.stamps_live;
  }
  Context& context;
};

template <typename Status>
struct StatusFactory;
template <>
struct StatusFactory<talon::Status> {
  static talon::Status Good() { return talon::OkStatus(); }
  static talon::Status Error(const Context& context) {
    return talon::Status(context.code, context.message);
  }
};
template <>
struct StatusFactory<absl::Status> {
  static absl::Status Good() { return absl::OkStatus(); }
  static absl::Status Error(const Context& context) {
    absl::Status error =
        talon::ToAbslStatus(talon::Status(context.code, context.message));
    error.SetPayload(kPayloadUrl, absl::Cord("long-chain-payload"));
    return error;
  }
};

template <typename Status>
Status Enter(Context& context, int level) {
  ++context.entries;
  if (context.phase == kEntryError && context.failure == level)
    return StatusFactory<Status>::Error(context);
  return StatusFactory<Status>::Good();
}

template <typename Status>
Status Complete(Context& context, int level, Value& value) {
  ++context.exits;
  if (context.phase == kExitError && context.failure == level)
    return StatusFactory<Status>::Error(context);
  value.score += static_cast<std::uint64_t>(level + 1);
  ++context.completed;
  return StatusFactory<Status>::Good();
}

absl::StatusOr<std::unique_ptr<Stamp>> Audit(Context& context) {
  ++context.audit_calls;
  return std::unique_ptr<Stamp>(new Stamp(context));
}

void Scan(Context& context) {
  for (int item = 0; item != 3; ++item) {
    CONTINUE_IF_ERROR((
        ++context.item_calls,
        item % 2 == 0 ? absl::StatusOr<int>(item)
                      : absl::StatusOr<int>(absl::NotFoundError("discarded"))));
    ++context.item_successes;
  }
}

template <typename Function>
auto Invoke(Function&& function)
    -> decltype(std::forward<Function>(function)()) {
  return std::forward<Function>(function)();
}

Pointer& AssignmentTarget(Context& context, Pointer& value) {
  ++context.assignments;
  return value;
}

template <typename Status, typename Result>
Result Leaf(Context& context, int level) {
  Pointer value(new Value(context));
  RETURN_IF_ERROR(Complete<Status>(context, level, *value));
  return value;
}

TalonResult TalonValue(Context& context, int remaining, int level);
absl::Status AbslStatus(Context& context, int remaining, int level,
                        Pointer* output);
talon::Status TalonStatus(Context& context, int remaining, int level,
                          Pointer* output);
AbslResult AbslValue(Context& context, int remaining, int level);

// Four different edges exercise all status/result source and destination kinds:
// TalonResult <- AbslStatus <- TalonStatus <- AbslResult <- TalonResult.
TalonResult TalonValue(Context& context, int remaining, int level) {
  Frame frame(context);
  RETURN_IF_ERROR(Enter<talon::Status>(context, level));
  RETURN_IF_ERROR(Audit(context));
  CHECK_EQ(context.stamps_live, 0);
  if (remaining == 0) return Leaf<talon::Status, TalonResult>(context, level);
  Pointer value;
  RETURN_IF_ERROR((++context.child_calls,
                   AbslStatus(context, remaining - 1, level + 1, &value)));
  RETURN_IF_ERROR(Complete<talon::Status>(context, level, *value));
  return value;
}

absl::Status AbslStatus(Context& context, int remaining, int level,
                        Pointer* output) {
  Frame frame(context);
  RETURN_IF_ERROR(Enter<absl::Status>(context, level));
  Scan(context);
  if (remaining == 0) {
    ASSIGN_OR_RETURN(Pointer value,
                     (Leaf<absl::Status, AbslResult>(context, level)));
    *output = std::move(value);
    return absl::OkStatus();
  }
  Pointer value;
  RETURN_IF_ERROR((++context.child_calls,
                   TalonStatus(context, remaining - 1, level + 1, &value)));
  RETURN_IF_ERROR(Complete<absl::Status>(context, level, *value));
  *output = std::move(value);
  return absl::OkStatus();
}

talon::Status TalonStatus(Context& context, int remaining, int level,
                          Pointer* output) {
  Frame frame(context);
  RETURN_IF_ERROR(Enter<talon::Status>(context, level));
  if (remaining == 0) {
    ASSIGN_OR_RETURN(Pointer value,
                     (Leaf<talon::Status, TalonResult>(context, level)));
    *output = std::move(value);
    return talon::OkStatus();
  }
  Pointer value;
  ASSIGN_OR_RETURN(AssignmentTarget(context, value),
                   (++context.child_calls, Invoke([&]() -> AbslResult {
                     return AbslValue(context, remaining - 1, level + 1);
                   })));
  RETURN_IF_ERROR(Complete<talon::Status>(context, level, *value));
  *output = std::move(value);
  return talon::OkStatus();
}

AbslResult AbslValue(Context& context, int remaining, int level) {
  Frame frame(context);
  RETURN_IF_ERROR(Enter<absl::Status>(context, level));
  if (remaining == 0) return Leaf<absl::Status, AbslResult>(context, level);
  const auto outer = [&]() -> TalonResult {
    return Invoke([&]() -> TalonResult {
      return TalonValue(context, remaining - 1, level + 1);
    });
  };
  ASSIGN_OR_RETURN(Pointer result, (++context.child_calls, Invoke(outer)));
  RETURN_IF_ERROR(Complete<absl::Status>(context, level, *result));
  return result;
}

AbslResult AbslOnlyValue(Context& context, int remaining, int level);
absl::Status AbslOnlyStatus(Context& context, int remaining, int level,
                            Pointer* output);

// A second long graph never converts out of Abseil; payloads must survive its
// alternating Status/StatusOr edges even when the error starts at the leaf.
AbslResult AbslOnlyValue(Context& context, int remaining, int level) {
  Frame frame(context);
  RETURN_IF_ERROR(Enter<absl::Status>(context, level));
  RETURN_IF_ERROR(Audit(context));
  if (remaining == 0) return Leaf<absl::Status, AbslResult>(context, level);
  Pointer value;
  RETURN_IF_ERROR((++context.child_calls,
                   AbslOnlyStatus(context, remaining - 1, level + 1, &value)));
  RETURN_IF_ERROR(Complete<absl::Status>(context, level, *value));
  return value;
}

absl::Status AbslOnlyStatus(Context& context, int remaining, int level,
                            Pointer* output) {
  Frame frame(context);
  RETURN_IF_ERROR(Enter<absl::Status>(context, level));
  Scan(context);
  if (remaining == 0) {
    ASSIGN_OR_RETURN(Pointer value,
                     (Leaf<absl::Status, AbslResult>(context, level)));
    *output = std::move(value);
    return absl::OkStatus();
  }
  Pointer value;
  ASSIGN_OR_RETURN(AssignmentTarget(context, value),
                   (++context.child_calls,
                    AbslOnlyValue(context, remaining - 1, level + 1)));
  RETURN_IF_ERROR(Complete<absl::Status>(context, level, *value));
  *output = std::move(value);
  return absl::OkStatus();
}

talon::Status Canonical(const talon::Status& status) { return status; }
talon::Status Canonical(const absl::Status& status) {
  return talon::ToTalonStatus(status);
}
bool Payload(const talon::Status&) { return false; }
bool Payload(const absl::Status& status) {
  const auto payload = status.GetPayload(kPayloadUrl);
  if (payload) CHECK(*payload == absl::Cord("long-chain-payload"));
  return payload.has_value();
}

template <typename Status>
void CheckStatus(const Status& status, const Context& context,
                 bool expected_payload) {
  CHECK_EQ(status.ok(), context.phase == kSuccess);
  CHECK_EQ(Payload(status), expected_payload);
  if (!status.ok()) {
    const talon::Status canonical = Canonical(status);
    CHECK_EQ(canonical.code(), context.code);
    CHECK_EQ(canonical.message(), context.message);
    CHECK(canonical.message().size() > 1024);
  }
}

// Count residue classes in a prefix; the oracle does not call the production
// graph or propagate statuses to calculate its expected event counts.
int CountRole(int end, int period, int residue) {
  return end <= residue ? 0 : 1 + (end - 1 - residue) / period;
}

void CheckCounters(const Context& context, bool mixed, int root) {
  const int nodes = context.depth + 1;
  const bool entry_error = context.phase == kEntryError;
  const bool success = context.phase == kSuccess;
  const int visited = entry_error ? context.failure + 1 : nodes;
  const int entered = entry_error ? context.failure : nodes;
  const int completed =
      success ? nodes : (entry_error ? 0 : context.depth - context.failure);
  const int exits =
      success ? nodes : (entry_error ? 0 : context.depth - context.failure + 1);
  CHECK_EQ(context.entries, visited);
  CHECK_EQ(context.exits, exits);
  CHECK_EQ(context.child_calls, entry_error ? context.failure : context.depth);
  CHECK_EQ(context.completed, completed);
  CHECK_EQ(context.frames_created, visited);
  CHECK_EQ(context.frames_destroyed, visited);
  CHECK_EQ(context.frames_live, 0);
  CHECK_EQ(context.maximum_frames, visited);
  CHECK_EQ(context.values_created, entry_error ? 0 : 1);
  CHECK_EQ(context.values_destroyed, context.values_created);
  CHECK_EQ(context.values_live, 0);
  const int period = mixed ? 4 : 2;
  const int audit_role = mixed ? (4 - root) % 4 : 0;
  const int scan_role = mixed ? (5 - root) % 4 : 1;
  const int assignment_role = mixed ? (6 - root) % 4 : 1;
  CHECK_EQ(context.audit_calls, CountRole(entered, period, audit_role));
  CHECK_EQ(context.stamps_created, context.audit_calls);
  CHECK_EQ(context.stamps_destroyed, context.audit_calls);
  CHECK_EQ(context.stamps_live, 0);
  CHECK_EQ(context.item_calls, 3 * CountRole(entered, period, scan_role));
  CHECK_EQ(context.item_successes, 2 * CountRole(entered, period, scan_role));
  const int first_assignment = success ? 0 : context.failure;
  CHECK_EQ(context.assignments,
           entry_error
               ? 0
               : CountRole(context.depth, period, assignment_role) -
                     CountRole(first_assignment, period, assignment_role));
}

void CheckCase(int depth, Phase phase, int failure, bool mixed, int root) {
  Context context(depth, phase, failure);
  // With an alternating graph, a non-root error must cross a system boundary.
  // An Abseil root error and every pure-Abseil error preserve their payloads.
  const bool expected_payload =
      phase != kSuccess && (!mixed || (failure == 0 && root % 2 == 1));
  Pointer value;
  if (!mixed) {
    AbslResult result = AbslOnlyValue(context, depth, 0);
    CheckStatus(result.status(), context, expected_payload);
    if (result.ok()) value = std::move(result).value();
  } else {
    switch (root) {
      case 0: {
        TalonResult result = TalonValue(context, depth, 0);
        CheckStatus(result.status(), context, expected_payload);
        if (result.ok()) value = std::move(result).value();
        break;
      }
      case 1: {
        const absl::Status status = AbslStatus(context, depth, 0, &value);
        CheckStatus(status, context, expected_payload);
        break;
      }
      case 2: {
        const talon::Status status = TalonStatus(context, depth, 0, &value);
        CheckStatus(status, context, expected_payload);
        break;
      }
      default: {
        AbslResult result = AbslValue(context, depth, 0);
        CheckStatus(result.status(), context, expected_payload);
        if (result.ok()) value = std::move(result).value();
        break;
      }
    }
  }
  CHECK_EQ(static_cast<bool>(value), phase == kSuccess);
  if (value) {
    const std::uint64_t nodes = static_cast<std::uint64_t>(depth + 1);
    CHECK_EQ(value->score, 7 + nodes * (nodes + 1) / 2);
    CHECK_EQ(context.values_live, 1);
  }
  value.reset();
  CheckCounters(context, mixed, root);
}

}  // namespace

int main() {
  // This is a bounded stack-depth test, not a guarantee of unlimited recursion.
  const int depths[] = {0, 1, 2, 3, 16, 256};
  for (const int depth : depths) {
    for (int root = 0; root != 4; ++root) {
      CheckCase(depth, kSuccess, 0, true, root);
      for (int level = 0; level <= depth; ++level) {
        CheckCase(depth, kEntryError, level, true, root);
        CheckCase(depth, kExitError, level, true, root);
      }
    }
    CheckCase(depth, kSuccess, 0, false, 0);
    for (int level = 0; level <= depth; ++level) {
      CheckCase(depth, kEntryError, level, false, 0);
      CheckCase(depth, kExitError, level, false, 0);
    }
  }
  return 0;
}
