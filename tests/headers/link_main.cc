// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include "talon/status.h"

talon::Status MultiTuCheck();

int main() {
  const talon::Status result = MultiTuCheck();
  return result == talon::NotFoundError("multi-tu") ? 0 : 1;
}
