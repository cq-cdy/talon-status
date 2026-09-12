cmake_minimum_required(VERSION 3.16)
include("${CMAKE_CURRENT_LIST_DIR}/NestedBuild.cmake")
file(STRINGS "${TALON_TEST_SOURCE}" expectation REGEX "^// EXPECT: " LIMIT_COUNT 1)
if(NOT expectation)
  message(FATAL_ERROR "Compile-fail test requires // EXPECT: diagnostic regex: ${TALON_TEST_SOURCE}")
endif()
string(REGEX REPLACE "^// EXPECT: " "" expectation "${expectation}")
file(STRINGS "${TALON_TEST_SOURCE}" standard REGEX "^// STANDARD: " LIMIT_COUNT 1)
if(standard)
  string(REGEX REPLACE "^// STANDARD: " "" standard "${standard}")
  list(APPEND talon_nested_configure_args "-DCMAKE_CXX_STANDARD=${standard}")
endif()
file(STRINGS "${TALON_TEST_SOURCE}" backend REGEX "^// BACKEND: " LIMIT_COUNT 1)
if(backend)
  string(REGEX REPLACE "^// BACKEND: " "" TALON_BACKEND_VALUE "${backend}")
endif()
file(STRINGS "${TALON_TEST_SOURCE}" no_exceptions REGEX "^// NO_EXCEPTIONS" LIMIT_COUNT 1)
get_filename_component(test_name "${TALON_TEST_SOURCE}" NAME_WE)
set(test_build "${TALON_BUILD_DIR}/compile-fail/${test_name}")
talon_require_success("Configure ${test_name}"
  "${CMAKE_COMMAND}" -S "${TALON_SOURCE_DIR}/cmake/compile_fail" -B "${test_build}"
  ${talon_nested_configure_args}
  "-DTALON_INCLUDE_DIR=${TALON_SOURCE_DIR}/include"
  "-DTALON_TEST_SOURCE=${TALON_TEST_SOURCE}"
  "-DTALON_BACKEND_VALUE=${TALON_BACKEND_VALUE}"
  "-DTALON_NO_EXCEPTIONS=${no_exceptions}")
talon_require_success("Compiler baseline for ${test_name}"
  "${CMAKE_COMMAND}" --build "${test_build}" ${talon_nested_build_args} --target baseline)
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${test_build}"
  ${talon_nested_build_args} --target rejected
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
set(diagnostic "${output}\n${error}")
if(result EQUAL 0)
  message(FATAL_ERROR "Expected compilation to fail, but ${test_name} compiled successfully")
endif()
if(NOT diagnostic MATCHES "${expectation}")
  message(FATAL_ERROR
    "${test_name} failed for an unexpected reason. Expected /${expectation}/:\n${diagnostic}")
endif()
message(STATUS "${test_name}: rejected with the expected diagnostic /${expectation}/")
