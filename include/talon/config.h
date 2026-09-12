// Copyright 2026 Talon Status Contributors. SPDX-License-Identifier: Apache-2.0
#ifndef TALON_CONFIG_H_
#define TALON_CONFIG_H_

// 0: automatic; 1: std::expected; 2: bundled C++11 storage.
#ifndef TALON_STATUS_BACKEND
#define TALON_STATUS_BACKEND 0
#endif
#if TALON_STATUS_BACKEND < 0 || TALON_STATUS_BACKEND > 2
#error "TALON_STATUS_BACKEND must be 0 (AUTO), 1 (STD), or 2 (COMPAT)"
#endif

#if defined(_MSVC_LANG)
#define TALON_STATUS_INTERNAL_LANGUAGE _MSVC_LANG
#else
#define TALON_STATUS_INTERNAL_LANGUAGE __cplusplus
#endif
#if TALON_STATUS_INTERNAL_LANGUAGE < 201103L
#error "talon-status requires C++11 or later"
#endif
#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS) && !defined(_CPPUNWIND)
#error "talon-status requires C++ exceptions enabled"
#endif

// A language mode alone does not prove that the standard library has expected.
#if TALON_STATUS_INTERNAL_LANGUAGE > 202002L && defined(__has_include)
#if __has_include(<expected>)
#include <expected>
#endif
#endif
#if TALON_STATUS_INTERNAL_LANGUAGE > 202002L && defined(__cpp_lib_expected) && \
    __cpp_lib_expected >= 202202L
#define TALON_STATUS_HAS_STD_EXPECTED 1
#else
#define TALON_STATUS_HAS_STD_EXPECTED 0
#endif
#if TALON_STATUS_BACKEND == 1 && !TALON_STATUS_HAS_STD_EXPECTED
#error "TALON STD backend requires C++23 and __cpp_lib_expected >= 202202L"
#endif
#if TALON_STATUS_BACKEND == 1 || \
    (TALON_STATUS_BACKEND == 0 && TALON_STATUS_HAS_STD_EXPECTED)
#define TALON_STATUS_USE_STD_EXPECTED 1
#else
#define TALON_STATUS_USE_STD_EXPECTED 0
#endif

#endif  // TALON_CONFIG_H_
