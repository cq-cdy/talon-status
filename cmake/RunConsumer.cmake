cmake_minimum_required(VERSION 3.16)
include("${CMAKE_CURRENT_LIST_DIR}/NestedBuild.cmake")
set(consumer_build "${TALON_BUILD_DIR}/consumer-${TALON_CONSUMER_MODE}")
set(consumer_args)
set(use_absl OFF)
if(TALON_CONSUMER_MODE MATCHES "^package(-absl)?$")
  set(install_prefix "${TALON_BUILD_DIR}/consumer-install-${TALON_CONSUMER_MODE}")
  talon_require_success("Install package for independent consumer"
    "${CMAKE_COMMAND}" --install "${TALON_BUILD_DIR}"
    ${talon_nested_build_args} --prefix "${install_prefix}")
  list(APPEND consumer_args "-DCMAKE_PREFIX_PATH=${install_prefix}")
  if(TALON_CONSUMER_MODE STREQUAL "package-absl")
    set(use_absl ON)
    list(APPEND consumer_args "-Dabsl_DIR=${TALON_ABSL_DIR}")
  endif()
elseif(TALON_CONSUMER_MODE STREQUAL "subdirectory")
  list(APPEND consumer_args "-DTALON_STATUS_SOURCE_DIR=${TALON_SOURCE_DIR}")
else()
  message(FATAL_ERROR "Unknown consumer mode: ${TALON_CONSUMER_MODE}")
endif()
if(use_absl)
  list(APPEND consumer_args -DCMAKE_DISABLE_FIND_PACKAGE_absl:BOOL=FALSE)
else()
  list(APPEND consumer_args -DCMAKE_DISABLE_FIND_PACKAGE_absl:BOOL=TRUE)
endif()
talon_require_success("Configure ${TALON_CONSUMER_MODE} consumer"
  "${CMAKE_COMMAND}" -S "${TALON_SOURCE_DIR}/tests/consumer" -B "${consumer_build}"
  ${talon_nested_configure_args} ${consumer_args}
  "-DTALON_STATUS_CONSUMER_MODE=${TALON_CONSUMER_MODE}"
  "-DTALON_STATUS_BACKEND=${TALON_BACKEND}"
  "-DTALON_STATUS_CXX_STANDARD=${TALON_CXX_STANDARD}"
  "-DTALON_STATUS_CONSUMER_ABSL=${use_absl}"
  -DTALON_STATUS_BUILD_TESTS:BOOL=OFF -DTALON_STATUS_BUILD_EXAMPLES:BOOL=OFF
  -DTALON_STATUS_WITH_ABSL:BOOL=OFF -DTALON_STATUS_ABSL_TESTS:BOOL=OFF)
talon_require_success("Build ${TALON_CONSUMER_MODE} consumer"
  "${CMAKE_COMMAND}" --build "${consumer_build}" ${talon_nested_build_args})
set(consumer_ctest_args --output-on-failure)
if(TALON_CONFIG)
  list(APPEND consumer_ctest_args -C "${TALON_CONFIG}")
endif()
execute_process(COMMAND "${CMAKE_CTEST_COMMAND}" ${consumer_ctest_args}
  WORKING_DIRECTORY "${consumer_build}" RESULT_VARIABLE result
  OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Run ${TALON_CONSUMER_MODE} consumer failed (${result}):\n${output}\n${error}")
endif()
message(STATUS "Run ${TALON_CONSUMER_MODE} consumer: passed")
