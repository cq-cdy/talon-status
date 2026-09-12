// EXPECT: CONTINUE_IF_ERROR is already defined
// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#define CONTINUE_IF_ERROR(expression) existing_continue_macro(expression)
#include "talon/status_macros_short.h"
int main() {}
