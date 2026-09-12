// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include <iostream>
#include <vector>

#include "talon/status_macros_short.h"

namespace {
struct Value {
  int number;
};
struct Item {
  int number;
};
Value InitialValue() { return Value{0}; }
talon::StatusOr<Value> MakeValue() { return Value{42}; }
talon::Status Validate(const Value& value) {
  if (value.number < 0) {
    return talon::InvalidArgumentError("invalid size: ", value.number,
                                       ", expected: nonnegative");
  }
  return talon::OkStatus();
}
talon::StatusOr<Value> BuildValue() {
  Value value = InitialValue();
  ASSIGN_OR_RETURN(value, MakeValue());
  ASSIGN_OR_RETURN(Value new_value, MakeValue());
  RETURN_IF_ERROR(Validate(value));
  RETURN_IF_ERROR(Validate(new_value));
  return new_value;
}
talon::Status DoWork() {
  ASSIGN_OR_RETURN(Value value, BuildValue());
  std::cout << "Built value: " << value.number << '\n';
  return talon::OkStatus();
}
talon::Status Run() {
  RETURN_IF_ERROR(DoWork());
  return talon::OkStatus();
}
talon::Status ProcessOne(const Item& item) {
  return item.number < 0 ? talon::InvalidArgumentError("negative item")
                         : talon::OkStatus();
}
void OnSuccess(const Item& item) {
  std::cout << "Processed: " << item.number << '\n';
}
void ProcessItems(const std::vector<Item>& items) {
  for (const auto& item : items) {
    CONTINUE_IF_ERROR(ProcessOne(item));
    OnSuccess(item);
  }
}
}  // namespace

int main() {
  const talon::Status status = Run();
  if (!status.ok()) {
    std::cerr << status << '\n';
    return 1;
  }
  ProcessItems(std::vector<Item>{{1}, {-1}, {3}});
  return 0;
}
