// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include "talon/config.h"

static_assert(TALON_STATUS_USE_STD_EXPECTED == 0 ||
                  TALON_STATUS_USE_STD_EXPECTED == 1,
              "backend selection must be a boolean");
