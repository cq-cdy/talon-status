cmake_minimum_required(VERSION 3.16)
include("${CMAKE_CURRENT_LIST_DIR}/NestedBuild.cmake")
set(case_args -DTALON_STATUS_BUILD_TESTS=OFF -DTALON_STATUS_BUILD_EXAMPLES=OFF
  -DTALON_STATUS_WITH_ABSL=OFF -DTALON_STATUS_ABSL_TESTS=OFF
  -DTALON_STATUS_BACKEND=AUTO -DTALON_STATUS_CXX_STANDARD=11)
if(TALON_CONFIGURE_CASE STREQUAL "invalid_backend")
  list(APPEND case_args -DTALON_STATUS_BACKEND=INVALID)
  set(expectation "TALON_STATUS_BACKEND must be AUTO, STD, or COMPAT")
elseif(TALON_CONFIGURE_CASE STREQUAL "invalid_standard")
  list(APPEND case_args -DTALON_STATUS_CXX_STANDARD=12)
  set(expectation "TALON_STATUS_CXX_STANDARD must be")
elseif(TALON_CONFIGURE_CASE STREQUAL "std_too_old")
  list(APPEND case_args -DTALON_STATUS_BACKEND=STD)
  set(expectation "TALON_STATUS_BACKEND=STD requires TALON_STATUS_CXX_STANDARD=23")
elseif(TALON_CONFIGURE_CASE STREQUAL "absl_tests_without_core_tests")
  list(APPEND case_args -DTALON_STATUS_ABSL_TESTS=ON)
  set(expectation "TALON_STATUS_ABSL_TESTS requires TALON_STATUS_BUILD_TESTS=ON")
else()
  message(FATAL_ERROR "Unknown configure-fail case: ${TALON_CONFIGURE_CASE}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${TALON_SOURCE_DIR}"
  -B "${TALON_BUILD_DIR}/configure-fail/${TALON_CONFIGURE_CASE}"
  ${talon_nested_configure_args} ${case_args}
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(result EQUAL 0)
  message(FATAL_ERROR "Expected configuration to reject ${TALON_CONFIGURE_CASE}")
endif()
if(NOT "${output}\n${error}" MATCHES "${expectation}")
  message(FATAL_ERROR
    "Configuration failed for an unexpected reason. Expected /${expectation}/:\n${output}\n${error}")
endif()
message(STATUS "${TALON_CONFIGURE_CASE}: rejected with the expected diagnostic")
