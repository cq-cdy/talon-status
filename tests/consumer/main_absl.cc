// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#include "talon/absl_adapter.h"

talon::StatusOr<int> InstalledTalonResult() {
  TALON_RETURN_IF_ERROR(absl::InvalidArgumentError("installed-absl"));
  return 1;
}

absl::Status InstalledAbslResult() {
  TALON_RETURN_IF_ERROR(talon::NotFoundError("installed-talon"));
  return absl::OkStatus();
}

int main() {
  const talon::StatusOr<int> talon_result = InstalledTalonResult();
  const talon::Status absl_result = talon::ToTalonStatus(InstalledAbslResult());
  return talon_result.status() ==
                 talon::InvalidArgumentError("installed-absl") &&
             absl_result == talon::NotFoundError("installed-talon")
         ? 0
         : 1;
}
