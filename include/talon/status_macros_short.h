// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_STATUS_MACROS_SHORT_H_
#define TALON_STATUS_MACROS_SHORT_H_

#include "talon/status_macros.h"

#if defined(ASSIGN_OR_RETURN)
#error "ASSIGN_OR_RETURN is already defined; use talon/status_macros.h and TALON_ names"
#elif defined(RETURN_IF_ERROR)
#error "RETURN_IF_ERROR is already defined; use talon/status_macros.h and TALON_ names"
#elif defined(CONTINUE_IF_ERROR)
#error "CONTINUE_IF_ERROR is already defined; use talon/status_macros.h and TALON_ names"
#else
#define ASSIGN_OR_RETURN(lhs, expr) TALON_ASSIGN_OR_RETURN(lhs, expr)
#define RETURN_IF_ERROR(expr) TALON_RETURN_IF_ERROR(expr)
#define CONTINUE_IF_ERROR(expr) TALON_CONTINUE_IF_ERROR(expr)
#endif

#endif  // TALON_STATUS_MACROS_SHORT_H_
