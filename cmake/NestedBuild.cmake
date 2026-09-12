# Arguments common to configure steps launched by CTest. All settings remain local
# to test build directories; they do not modify the consuming project's flags.
set(talon_nested_configure_args
  -G "${TALON_GENERATOR}"
  "-DCMAKE_CXX_COMPILER:FILEPATH=${TALON_CXX_COMPILER}"
  "-DCMAKE_CXX_STANDARD:STRING=${TALON_CXX_STANDARD}"
  -DCMAKE_CXX_STANDARD_REQUIRED:BOOL=ON
  -DCMAKE_CXX_EXTENSIONS:BOOL=OFF
  "-DCMAKE_BUILD_TYPE:STRING=${TALON_CONFIG}")
if(TALON_GENERATOR_PLATFORM)
  list(APPEND talon_nested_configure_args -A "${TALON_GENERATOR_PLATFORM}")
endif()
if(TALON_GENERATOR_TOOLSET)
  list(APPEND talon_nested_configure_args -T "${TALON_GENERATOR_TOOLSET}")
endif()
foreach(setting IN ITEMS MAKE_PROGRAM CXX_FLAGS CXX_FLAGS_DEBUG CXX_FLAGS_RELEASE
    CXX_FLAGS_RELWITHDEBINFO CXX_FLAGS_MINSIZEREL TOOLCHAIN_FILE
    OSX_ARCHITECTURES OSX_SYSROOT OSX_DEPLOYMENT_TARGET SYSROOT MSVC_RUNTIME_LIBRARY)
  if(DEFINED TALON_${setting} AND NOT "${TALON_${setting}}" STREQUAL "")
    list(APPEND talon_nested_configure_args "-DCMAKE_${setting}=${TALON_${setting}}")
  endif()
endforeach()
set(talon_nested_build_args)
if(TALON_CONFIG)
  list(APPEND talon_nested_build_args --config "${TALON_CONFIG}")
endif()

function(talon_require_success description)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${description} failed (${result}):\n${output}\n${error}")
  endif()
  message(STATUS "${description}: passed")
endfunction()
