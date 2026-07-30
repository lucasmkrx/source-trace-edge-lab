add_library(sourcetrace_build_options INTERFACE)
target_compile_features(sourcetrace_build_options INTERFACE cxx_std_20)

if(MSVC)
  target_compile_options(sourcetrace_build_options INTERFACE /W4 /WX)
else()
  target_compile_options(
    sourcetrace_build_options
    INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror
  )
endif()

add_library(
  sourcetrace_core
  src/core/reducer.cpp
  src/core/trace.cpp
  simulator/deterministic_simulator.cpp
)
target_include_directories(sourcetrace_core PUBLIC include)
target_link_libraries(sourcetrace_core PUBLIC sourcetrace_build_options)

if(SOURCETRACE_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG 644821ce28cb25d7992a4d0375b1d83214392592
    GIT_SHALLOW FALSE
    SYSTEM
    EXCLUDE_FROM_ALL
  )
  FetchContent_MakeAvailable(Catch2)

  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  include(Catch)

  add_executable(sourcetrace_core_tests tests/core_tests.cpp)
  target_link_libraries(
    sourcetrace_core_tests
    PRIVATE sourcetrace_core Catch2::Catch2WithMain sourcetrace_build_options
  )
  catch_discover_tests(sourcetrace_core_tests)
endif()
