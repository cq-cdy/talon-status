// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include "talon/status.h"
#include "talon/status_macros.h"
#include "talon/status_or.h"

talon::StatusOr<int> MultiTuValue();
talon::Status MultiTuError();

talon::Status MultiTuCheck() {
  TALON_ASSIGN_OR_RETURN(int value, MultiTuValue());
  if (value != 19) return talon::InternalError("wrong cross-TU value");
  TALON_RETURN_IF_ERROR(MultiTuError());
  return talon::InternalError("unreachable");
}
