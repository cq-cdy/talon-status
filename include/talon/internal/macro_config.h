// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_INTERNAL_MACRO_CONFIG_H_
#define TALON_INTERNAL_MACRO_CONFIG_H_

// This check is independent of the standard library so compiler front ends
// can validate the preprocessing requirement without a target SDK.
// clang-cl defines _MSC_VER but always uses Clang's conforming preprocessor.
#if defined(_MSC_VER) && !defined(__clang__) && \
    (!defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL)
#error \
    "Talon status macros require the conforming MSVC preprocessor (/Zc:preprocessor)"
#endif

#endif  // TALON_INTERNAL_MACRO_CONFIG_H_
