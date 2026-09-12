// EXPECT: RETURN_IF_ERROR is already defined
// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#define RETURN_IF_ERROR(expression) existing_return_macro(expression)
#include "talon/status_macros_short.h"
int main() {}
