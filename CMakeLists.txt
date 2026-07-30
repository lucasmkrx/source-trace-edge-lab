cmake_minimum_required(VERSION 3.25)

project(
  SourceTraceEdgeLab
  VERSION 0.1.0
  DESCRIPTION "Deterministic C++20 classroom edge-controller portfolio prototype"
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SOURCETRACE_BUILD_TESTS "Build behavioral tests" ON)
option(SOURCETRACE_BUILD_FUZZER "Build the libFuzzer target" OFF)
option(SOURCETRACE_ENABLE_CLANG_TIDY "Run clang-tidy during compilation" OFF)

if(SOURCETRACE_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
  set(SOURCETRACE_CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}")
endif()

function(sourcetrace_enable_project_tidy target)
  if(SOURCETRACE_ENABLE_CLANG_TIDY)
    set_property(
      TARGET "${target}" PROPERTY CXX_CLANG_TIDY "${SOURCETRACE_CLANG_TIDY_COMMAND}"
    )
  endif()
endfunction()

include(CTest)
add_subdirectory(edge)
