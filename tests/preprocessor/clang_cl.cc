// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#if !defined(__clang__) || !defined(_MSC_VER)
#error "This probe requires the real clang-cl Windows-compatible front end"
#endif

#include "talon/internal/macro_config.h"

// Syntax-only front-end regression; no Windows SDK or runtime is involved.
int main() { return 0; }
