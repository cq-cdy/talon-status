// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include "talon/status_macros_short.h"

talon::StatusOr<int> ConsumerValue();

talon::Status CheckConsumer() {
  ASSIGN_OR_RETURN(int value, ConsumerValue());
  if (value != 42) return talon::InternalError("consumer received wrong value");
  RETURN_IF_ERROR(talon::OkStatus());
  return talon::OkStatus();
}

int main() { return CheckConsumer().ok() ? 0 : 1; }
