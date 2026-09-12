// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#define TALON_CHAIN_EXISTING_MACRO(number) ((number) + 1)
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "talon/status_macros_short.h"
#include "test.h"

namespace talon_chain_test {
namespace {

enum Phase { kSuccess, kEntryError, kExitError, kExitException };

std::string FailureMessage(int level, Phase phase) {
  std::string message = "chain/level=" + std::to_string(level) +
                        "/phase=" + std::to_string(static_cast<int>(phase));
  message.push_back('\0');
  message += std::string(1024, static_cast<char>('a' + level % 26));
  message += "/end";
  return message;
}

struct Context {
  Context(int depth, Phase phase, int failing_level)
      : depth(depth),
        phase(phase),
        failing_level(failing_level),
        error_code(static_cast<talon::StatusCode>(1 + failing_level % 16)),
        error_message(FailureMessage(failing_level, phase)) {}

  int depth;
  Phase phase;
  int failing_level;
  talon::StatusCode error_code;
  std::string error_message;
  int entry_calls = 0;
  int exit_calls = 0;
  int child_calls = 0;
  int completed = 0;
  int frames_created = 0;
  int frames_destroyed = 0;
  int frames_live = 0;
  int maximum_frames = 0;
  int values_created = 0;
  int values_destroyed = 0;
  int values_live = 0;
  int audits_created = 0;
  int audits_destroyed = 0;
  int audits_live = 0;
  int audit_calls = 0;
  int item_calls = 0;
  int item_successes = 0;
  int metadata_calls = 0;
  int assignment_calls = 0;
  int root_calls = 0;
  int root_completed = 0;
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

struct AuditToken {
  explicit AuditToken(Context& context) : context(context) {
    ++context.audits_created;
    ++context.audits_live;
  }
  ~AuditToken() {
    ++context.audits_destroyed;
    --context.audits_live;
  }
  Context& context;
};

struct ChainException {
  explicit ChainException(int level) : level(level) {}
  int level;
};

talon::Status Enter(Context& context, int level) {
  ++context.entry_calls;
  if (context.phase == kEntryError && level == context.failing_level)
    return talon::Status(context.error_code, context.error_message);
  return talon::OkStatus();
}

talon::StatusOr<int> EnterResult(Context& context, int level) {
  RETURN_IF_ERROR(Enter(context, level));
  return level;
}

talon::Status Complete(Context& context, int level, Value& value) {
  ++context.exit_calls;
  if (level == context.failing_level) {
    if (context.phase == kExitError)
      return talon::Status(context.error_code, context.error_message);
    if (context.phase == kExitException) throw ChainException(level);
  }
  value.score += static_cast<std::uint64_t>(level + 1);
  ++context.completed;
  return talon::OkStatus();
}

talon::StatusOr<std::unique_ptr<AuditToken>> Audit(Context& context) {
  ++context.audit_calls;
  return std::unique_ptr<AuditToken>(new AuditToken(context));
}

talon::Status ItemStatus(Context& context, int item) {
  ++context.item_calls;
  return item % 2 == 0 ? talon::OkStatus()
                       : talon::UnavailableError("discarded item");
}

template <typename A, typename B>
talon::StatusOr<std::pair<A, B>> Metadata(Context& context, A level,
                                          B remaining) {
  ++context.metadata_calls;
  return std::make_pair(level, remaining);
}

template <typename Function>
auto Invoke(Function&& function)
    -> decltype(std::forward<Function>(function)()) {
  return std::forward<Function>(function)();
}

Pointer& AssignmentTarget(Context& context, Pointer& value) {
  ++context.assignment_calls;
  return value;
}

talon::StatusOr<Pointer> Leaf(Context& context, int level) {
  Pointer result(new Value(context));
  RETURN_IF_ERROR(Complete(context, level, *result));
  return result;
}

talon::StatusOr<Pointer> Fetch(Context& context, int remaining, int level);
talon::Status Inspect(Context& context, int remaining, int level,
                      Pointer* output);
talon::StatusOr<Pointer> Assemble(Context& context, int remaining, int level);

class Transformer {
 public:
  explicit Transformer(Context& context) : context_(context) {}
  talon::StatusOr<Pointer> operator()(int remaining, int level) const {
    Frame frame(context_);
    RETURN_IF_ERROR(Enter(context_, level));
    for (int item = 0; item != 4; ++item) {
      switch (item % 2) {
        case 0:
          CONTINUE_IF_ERROR(ItemStatus(context_, item));
          break;
        default:
          if (item < 4) CONTINUE_IF_ERROR(ItemStatus(context_, item));
          CHECK(false);
          break;
      }
      ++context_.item_successes;
    }
    if (remaining == 0) return Leaf(context_, level);
    Pointer result;
    RETURN_IF_ERROR((++context_.child_calls,
                     Inspect(context_, remaining - 1, level + 1, &result)));
    RETURN_IF_ERROR(Complete(context_, level, *result));
    return result;
  }

 private:
  Context& context_;
};

talon::StatusOr<Pointer> Fetch(Context& context, int remaining, int level) {
  Frame frame(context);
  RETURN_IF_ERROR(Enter(context, level));
  // Audit owns a move-only result which RETURN intentionally discards.
  switch (level % 3) {
    case 0:
      RETURN_IF_ERROR(Audit(context));
      break;
    case 1: {
      const auto validate = [&]() -> talon::Status {
        RETURN_IF_ERROR(Audit(context));
        return talon::OkStatus();
      };
      RETURN_IF_ERROR(Invoke(validate));
      break;
    }
    default:
      if (remaining >= 0)
        RETURN_IF_ERROR(Audit(context));
      else
        CHECK(false);
      break;
  }
  CHECK_EQ(context.audits_live, 0);
  if (remaining == 0) return Leaf(context, level);
  const Transformer transform(context);
  const int status = 11;
  ASSIGN_OR_RETURN(
      Pointer result,
      (++context.child_calls, Invoke([&]() -> talon::StatusOr<Pointer> {
        return transform(remaining - 1, level + 1);
      })));
  CHECK_EQ(status, 11);
  RETURN_IF_ERROR(Complete(context, level, *result));
  return result;
}

talon::Status Inspect(Context& context, int remaining, int level,
                      Pointer* output) {
  Frame frame(context);
  RETURN_IF_ERROR(Enter(context, level));
  ASSIGN_OR_RETURN((std::pair<int, int> result),
                   (Metadata<int, int>(context, level, remaining)));
  CHECK_EQ(result.first + result.second, context.depth);
  if (remaining == 0) {
    ASSIGN_OR_RETURN(Pointer value, Leaf(context, level));
    *output = std::move(value);
    return talon::OkStatus();
  }
  Pointer value;
  ASSIGN_OR_RETURN(
      AssignmentTarget(context, value),
      (++context.child_calls, Assemble(context, remaining - 1, level + 1)));
  RETURN_IF_ERROR(Complete(context, level, *value));
  *output = std::move(value);
  return talon::OkStatus();
}

talon::StatusOr<Pointer> Assemble(Context& context, int remaining, int level) {
  Frame frame(context);
  RETURN_IF_ERROR(EnterResult(context, level));
  if (remaining == 0) return Leaf(context, level);
  const auto outer = [&]() -> talon::StatusOr<Pointer> {
    return Invoke([&]() -> talon::StatusOr<Pointer> {
      Pointer value;
      ASSIGN_OR_RETURN(
          AssignmentTarget(context, value),
          (++context.child_calls, Fetch(context, remaining - 1, level + 1)));
      return value;
    });
  };
  ASSIGN_OR_RETURN(Pointer result, Invoke(outer));
  RETURN_IF_ERROR(Complete(context, level, *result));
  return result;
}

talon::StatusOr<Pointer> Run(Context& context) {
  ASSIGN_OR_RETURN(Pointer result,
                   (++context.root_calls, Fetch(context, context.depth, 0)));
  ++context.root_completed;
  return result;
}

// Independent oracle: nodes are numbered [0, depth]. Entry failure visits a
// prefix; exit failure/exception completes a suffix. Role counts are residue
// counts over that interval, rather than another execution of the call graph.
int RoleCount(int end, int role) { return end / 4 + (end % 4 > role ? 1 : 0); }

void CheckCounters(const Context& context) {
  const int nodes = context.depth + 1;
  const bool entry_failure = context.phase == kEntryError;
  const bool success = context.phase == kSuccess;
  const int visited = entry_failure ? context.failing_level + 1 : nodes;
  const int entered = entry_failure ? context.failing_level : nodes;
  const int completed =
      success ? nodes
              : (entry_failure ? 0 : context.depth - context.failing_level);
  const int exits =
      success ? nodes
              : (entry_failure ? 0 : context.depth - context.failing_level + 1);
  CHECK_EQ(context.entry_calls, visited);
  CHECK_EQ(context.exit_calls, exits);
  CHECK_EQ(context.child_calls,
           entry_failure ? context.failing_level : context.depth);
  CHECK_EQ(context.completed, completed);
  CHECK_EQ(context.frames_created, visited);
  CHECK_EQ(context.frames_destroyed, visited);
  CHECK_EQ(context.frames_live, 0);
  CHECK_EQ(context.maximum_frames, visited);
  CHECK_EQ(context.values_created, entry_failure ? 0 : 1);
  CHECK_EQ(context.values_destroyed, context.values_created);
  CHECK_EQ(context.values_live, 0);
  CHECK_EQ(context.audit_calls, RoleCount(entered, 0));
  CHECK_EQ(context.audits_created, context.audit_calls);
  CHECK_EQ(context.audits_destroyed, context.audit_calls);
  CHECK_EQ(context.audits_live, 0);
  CHECK_EQ(context.item_calls, 4 * RoleCount(entered, 1));
  CHECK_EQ(context.item_successes, 2 * RoleCount(entered, 1));
  CHECK_EQ(context.metadata_calls, RoleCount(entered, 2));
  const int first_assignment = success ? 0 : context.failing_level;
  const int assignments =
      entry_failure
          ? 0
          : RoleCount(context.depth, 2) + RoleCount(context.depth, 3) -
                RoleCount(first_assignment, 2) - RoleCount(first_assignment, 3);
  CHECK_EQ(context.assignment_calls, assignments);
  CHECK_EQ(context.root_calls, 1);
  CHECK_EQ(context.root_completed, success ? 1 : 0);
}

void CheckCase(int depth, Phase phase, int failing_level) {
  Context context(depth, phase, failing_level);
  if (phase == kExitException) {
    bool caught = false;
    try {
      (void)Run(context);
    } catch (const ChainException& exception) {
      CHECK_EQ(exception.level, failing_level);
      caught = true;
    }
    CHECK(caught);
  } else {
    const talon::StatusOr<Pointer> result = Run(context);
    CHECK_EQ(result.ok(), phase == kSuccess);
    if (result.ok()) {
      const std::uint64_t nodes = static_cast<std::uint64_t>(depth + 1);
      CHECK_EQ((*result)->score, 7 + nodes * (nodes + 1) / 2);
      CHECK_EQ(context.values_live, 1);
    } else {
      CHECK_EQ(result.status().code(), context.error_code);
      CHECK_EQ(result.status().message(), context.error_message);
      CHECK(result.status().message().size() > 1024);
      CHECK_EQ(context.values_live, 0);
    }
  }
  CheckCounters(context);
}

}  // namespace
}  // namespace talon_chain_test

int main() {
  CHECK_EQ(TALON_CHAIN_EXISTING_MACRO(2), 3);
  // 256 edges are deliberately bounded; available stack space is finite and
  // these tests do not promise arbitrary recursion depth for user programs.
  const int depths[] = {0, 1, 2, 3, 4, 31, 256};
  for (const int depth : depths) {
    talon_chain_test::CheckCase(depth, talon_chain_test::kSuccess, 0);
    for (int level = 0; level <= depth; ++level) {
      talon_chain_test::CheckCase(depth, talon_chain_test::kEntryError, level);
      talon_chain_test::CheckCase(depth, talon_chain_test::kExitError, level);
      talon_chain_test::CheckCase(depth, talon_chain_test::kExitException,
                                  level);
    }
  }
  return 0;
}
