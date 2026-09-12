// EXPECT: Talon status macros require the conforming MSVC preprocessor
#ifndef _MSC_VER
#define _MSC_VER 1930
#endif
#ifdef _MSVC_TRADITIONAL
#undef _MSVC_TRADITIONAL
#endif
#define _MSVC_TRADITIONAL 1
#ifdef __clang__
// This file deliberately exercises the non-Clang MSVC branch even when the
// surrounding matrix compiler is Clang. macro_config.h includes no SDK headers.
#undef __clang__
#endif

#include "talon/internal/macro_config.h"

int main() {}
