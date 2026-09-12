// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include "talon/status_or.h"
#include "talon/status_macros.h"
#include "talon/status.h"

talon::StatusOr<int> MultiTuValue() { return 19; }
talon::Status MultiTuError() { return talon::NotFoundError("multi-tu"); }
