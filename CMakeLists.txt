cmake_minimum_required(VERSION 3.16)
project(talon-status VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(TALON_STATUS_BACKEND "AUTO" CACHE STRING "StatusOr backend: AUTO, STD, or COMPAT")
set_property(CACHE TALON_STATUS_BACKEND PROPERTY STRINGS AUTO STD COMPAT)
if(TALON_STATUS_BACKEND STREQUAL "AUTO")
  set(talon_status_backend_value 0)
elseif(TALON_STATUS_BACKEND STREQUAL "STD")
  set(talon_status_backend_value 1)
elseif(TALON_STATUS_BACKEND STREQUAL "COMPAT")
  set(talon_status_backend_value 2)
else()
  message(FATAL_ERROR "TALON_STATUS_BACKEND must be AUTO, STD, or COMPAT")
endif()

if(TALON_STATUS_BACKEND STREQUAL "STD")
  set(talon_status_default_standard 23)
else()
  set(talon_status_default_standard 11)
endif()
set(TALON_STATUS_CXX_STANDARD "${talon_status_default_standard}" CACHE STRING
    "C++ standard for Talon's own tests and examples")
set_property(CACHE TALON_STATUS_CXX_STANDARD PROPERTY STRINGS 11 14 17 20 23)
if(NOT TALON_STATUS_CXX_STANDARD MATCHES "^(11|14|17|20|23)$")
  message(FATAL_ERROR "TALON_STATUS_CXX_STANDARD must be 11, 14, 17, 20, or 23")
endif()
if(TALON_STATUS_CXX_STANDARD EQUAL 23 AND CMAKE_VERSION VERSION_LESS 3.20)
  message(FATAL_ERROR "C++23 configuration requires CMake 3.20 or newer")
endif()

option(TALON_STATUS_BUILD_TESTS "Build Talon's tests (no downloaded test framework)" OFF)
option(TALON_STATUS_BUILD_EXAMPLES "Build Talon's examples" OFF)
option(TALON_STATUS_BUILD_BENCHMARKS "Build the optional dependency-free microbenchmark" OFF)
option(TALON_STATUS_WITH_ABSL "Provide the optional target talon::status_absl" OFF)
option(TALON_STATUS_ABSL_TESTS "Build tests using an installed, real Abseil" OFF)
option(TALON_STATUS_STRICT_WARNINGS "Enable strict warnings on Talon's own targets" OFF)
option(TALON_STATUS_SANITIZERS "Enable ASan and UBSan on Talon's test targets" OFF)
option(TALON_STATUS_COVERAGE "Enable GCC coverage instrumentation on test targets" OFF)

if(TALON_STATUS_ABSL_TESTS AND NOT TALON_STATUS_BUILD_TESTS)
  message(FATAL_ERROR "TALON_STATUS_ABSL_TESTS requires TALON_STATUS_BUILD_TESTS=ON")
endif()

add_library(talon_status INTERFACE)
add_library(talon::status ALIAS talon_status)
set_target_properties(talon_status PROPERTIES EXPORT_NAME status)
target_include_directories(talon_status INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(talon_status INTERFACE cxx_std_11)
target_compile_options(talon_status INTERFACE
  $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>)
target_compile_definitions(talon_status INTERFACE
  TALON_STATUS_BACKEND=${talon_status_backend_value})

if(TALON_STATUS_BACKEND STREQUAL "STD")
  if(TALON_STATUS_CXX_STANDARD LESS 23)
    message(FATAL_ERROR "TALON_STATUS_BACKEND=STD requires TALON_STATUS_CXX_STANDARD=23")
  endif()
  # This is a real compiler/library probe. A language version alone is insufficient.
  include(CheckCXXSourceCompiles)
  function(talon_status_probe_expected)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    # Re-check after flag/library changes in an existing build directory.
    unset(TALON_STATUS_HAS_STD_EXPECTED CACHE)
    check_cxx_source_compiles("#include <expected>
      #if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
      #error std::expected feature test is unavailable
      #endif
      int main() { std::expected<int, int> result(7); return result.value() - 7; }"
      TALON_STATUS_HAS_STD_EXPECTED)
  endfunction()
  talon_status_probe_expected()
  if(NOT TALON_STATUS_HAS_STD_EXPECTED)
    message(FATAL_ERROR
      "TALON_STATUS_BACKEND=STD requires working C++23 <expected> with __cpp_lib_expected >= 202202L; use AUTO or COMPAT on this toolchain")
  endif()
  target_compile_features(talon_status INTERFACE cxx_std_23)
endif()

if(TALON_STATUS_WITH_ABSL OR TALON_STATUS_ABSL_TESTS)
  find_package(absl CONFIG REQUIRED)
  add_library(talon_status_absl INTERFACE)
  add_library(talon::status_absl ALIAS talon_status_absl)
  target_link_libraries(talon_status_absl INTERFACE
    talon::status absl::status absl::statusor)
endif()

install(TARGETS talon_status EXPORT talon-status-targets)
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/talon-status")
install(EXPORT talon-status-targets
  FILE talon-status-targets.cmake NAMESPACE talon::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/talon-status")
configure_package_config_file(cmake/talon-status-config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/talon-status-config.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/talon-status")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/talon-status-config-version.cmake"
  VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/talon-status-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/talon-status-config-version.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/talon-status")

function(talon_status_project_target target)
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD "${TALON_STATUS_CXX_STANDARD}"
    CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
  target_link_libraries(${target} PRIVATE talon::status)
  if(TALON_STATUS_STRICT_WARNINGS)
    if(MSVC)
      target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
      target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
  endif()
endfunction()

function(talon_status_test_target target)
  talon_status_project_target(${target})
  # Allocation fault injection replaces global new/delete in its own process.
  # Clang's static ASan runtime also defines these symbols. Keep that test
  # running with its allocator, separately from the instrumented lifetime tests.
  if(TALON_STATUS_SANITIZERS AND NOT target STREQUAL "talon_allocation_test")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang" AND NOT MSVC)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target} PRIVATE -fsanitize=address,undefined)
      if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
        # Clang12 predates -fsanitize-address-use-after-return=runtime.
        target_compile_options(${target} PRIVATE -mllvm -asan-use-after-return=true)
      endif()
    else()
      message(FATAL_ERROR "TALON_STATUS_SANITIZERS currently requires GCC or Clang with ASan and UBSan")
    endif()
  endif()
  if(TALON_STATUS_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${target} PRIVATE --coverage -O0 -g)
      target_link_options(${target} PRIVATE --coverage)
    else()
      message(FATAL_ERROR "TALON_STATUS_COVERAGE currently requires GCC/gcov")
    endif()
  endif()
endfunction()

if(TALON_STATUS_BUILD_TESTS)
  enable_testing()
  find_program(TALON_STATUS_CLANG_CL NAMES clang-cl clang-cl-20 clang-cl-19
    clang-cl-18 clang-cl-17 clang-cl-16 clang-cl-15 clang-cl-14 clang-cl-13
    clang-cl-12)
  mark_as_advanced(TALON_STATUS_CLANG_CL)
  if(TALON_STATUS_CLANG_CL)
    add_test(NAME talon.clang_cl_preprocessor
      COMMAND "${TALON_STATUS_CLANG_CL}" /nologo /Zs /TP /W4 /WX
        "/I${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/preprocessor/clang_cl.cc")
  endif()
  foreach(test_name IN ITEMS status status_or macros allocation call_chain)
    add_executable(talon_${test_name}_test "tests/${test_name}_test.cc")
    talon_status_test_target(talon_${test_name}_test)
    add_test(NAME "talon.${test_name}" COMMAND talon_${test_name}_test)
  endforeach()
  add_executable(talon_macros_line_test tests/macros_test.cc)
  talon_status_test_target(talon_macros_line_test)
  target_compile_definitions(talon_macros_line_test PRIVATE TALON_STATUS_USE_LINE_COUNTER=1)
  add_test(NAME talon.macros_line COMMAND talon_macros_line_test)
  add_executable(talon_call_chain_line_test tests/call_chain_test.cc)
  talon_status_test_target(talon_call_chain_line_test)
  target_compile_definitions(talon_call_chain_line_test PRIVATE TALON_STATUS_USE_LINE_COUNTER=1)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
    target_compile_options(talon_call_chain_line_test PRIVATE -fno-elide-constructors)
  endif()
  add_test(NAME talon.call_chain_line COMMAND talon_call_chain_line_test)

  add_library(talon_header_tests OBJECT
    tests/headers/config.cc tests/headers/status.cc tests/headers/status_or.cc
    tests/headers/status_macros.cc tests/headers/status_macros_short.cc)
  talon_status_test_target(talon_header_tests)
  add_executable(talon_multitu_test tests/headers/link_main.cc
    tests/headers/link_a.cc tests/headers/link_b.cc)
  talon_status_test_target(talon_multitu_test)
  add_test(NAME talon.multitu COMMAND talon_multitu_test)

  if(TALON_STATUS_ABSL_TESTS)
    add_executable(talon_absl_test tests/absl_test.cc)
    talon_status_test_target(talon_absl_test)
    target_link_libraries(talon_absl_test PRIVATE talon::status_absl)
    add_test(NAME talon.absl COMMAND talon_absl_test)
    add_executable(talon_absl_chain_test tests/absl_chain_test.cc)
    talon_status_test_target(talon_absl_chain_test)
    target_link_libraries(talon_absl_chain_test PRIVATE talon::status_absl)
    add_test(NAME talon.absl_chain COMMAND talon_absl_chain_test)
    add_library(talon_absl_header_test OBJECT tests/headers/absl_adapter.cc)
    talon_status_test_target(talon_absl_header_test)
    target_link_libraries(talon_absl_header_test PRIVATE talon::status_absl)
  endif()

  # Every nested build uses the same compiler, generator and relevant platform flags.
  set(talon_status_nested_args
    "-DTALON_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DTALON_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
    "-DTALON_GENERATOR=${CMAKE_GENERATOR}"
    "-DTALON_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DTALON_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
    "-DTALON_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
    "-DTALON_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DTALON_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
    "-DTALON_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}"
    "-DTALON_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}"
    "-DTALON_CXX_FLAGS_RELWITHDEBINFO=${CMAKE_CXX_FLAGS_RELWITHDEBINFO}"
    "-DTALON_CXX_FLAGS_MINSIZEREL=${CMAKE_CXX_FLAGS_MINSIZEREL}"
    "-DTALON_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    "-DTALON_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES}"
    "-DTALON_OSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
    "-DTALON_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
    "-DTALON_SYSROOT=${CMAKE_SYSROOT}"
    "-DTALON_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}"
    "-DTALON_CXX_STANDARD=${TALON_STATUS_CXX_STANDARD}"
    "-DTALON_BACKEND=${TALON_STATUS_BACKEND}"
    "-DTALON_BACKEND_VALUE=${talon_status_backend_value}"
    "-DTALON_ABSL_DIR=${absl_DIR}"
    "-DTALON_CONFIG=$<CONFIG>")
  file(GLOB talon_status_compile_fail_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fail/*.cc")
  foreach(source IN LISTS talon_status_compile_fail_sources)
    get_filename_component(test_name "${source}" NAME_WE)
    add_test(NAME "talon.compile_fail.${test_name}"
      COMMAND "${CMAKE_COMMAND}" ${talon_status_nested_args}
        "-DTALON_TEST_SOURCE=${source}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunCompileFail.cmake")
  endforeach()
  foreach(case IN ITEMS invalid_backend invalid_standard std_too_old absl_tests_without_core_tests)
    add_test(NAME "talon.configure_fail.${case}"
      COMMAND "${CMAKE_COMMAND}" ${talon_status_nested_args}
        "-DTALON_CONFIGURE_CASE=${case}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunConfigureFail.cmake")
  endforeach()
  foreach(mode IN ITEMS package subdirectory)
    add_test(NAME "talon.consumer.${mode}"
      COMMAND "${CMAKE_COMMAND}" ${talon_status_nested_args}
        "-DTALON_CONSUMER_MODE=${mode}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunConsumer.cmake")
  endforeach()
  if(TALON_STATUS_ABSL_TESTS)
    add_test(NAME talon.consumer.package_absl
      COMMAND "${CMAKE_COMMAND}" ${talon_status_nested_args}
        -DTALON_CONSUMER_MODE=package-absl
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunConsumer.cmake")
  endif()
endif()

if(TALON_STATUS_BUILD_EXAMPLES)
  add_executable(talon_status_example examples/basic.cc)
  talon_status_project_target(talon_status_example)
  if(TALON_STATUS_BUILD_TESTS)
    add_test(NAME talon.example COMMAND talon_status_example)
  endif()
endif()

if(TALON_STATUS_BUILD_BENCHMARKS)
  add_executable(talon_status_benchmark benchmarks/status_benchmark.cc benchmarks/workloads.cc)
  talon_status_project_target(talon_status_benchmark)
  if(TALON_STATUS_BUILD_TESTS)
    add_test(NAME talon.benchmark_smoke COMMAND talon_status_benchmark 128 1)
  endif()
endif()
