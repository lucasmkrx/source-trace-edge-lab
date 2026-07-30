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
sourcetrace_enable_project_tidy(sourcetrace_core)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG 55f93686c01528224f448c19128836e7df245f72
  GIT_SHALLOW FALSE
  SYSTEM
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

add_library(
  sourcetrace_protocol
  src/protocol/crc32.cpp
  src/protocol/frame.cpp
  src/protocol/parser.cpp
  src/protocol/payload_validation.cpp
  src/protocol/replay_window.cpp
  src/crypto/ed25519.cpp
)
target_include_directories(sourcetrace_protocol PUBLIC include)
target_link_libraries(
  sourcetrace_protocol
  PUBLIC
    sourcetrace_core
    sourcetrace_build_options
    nlohmann_json::nlohmann_json
    PkgConfig::SODIUM
)
sourcetrace_enable_project_tidy(sourcetrace_protocol)

if(SOURCETRACE_BUILD_TESTS)
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

  add_executable(sourcetrace_protocol_tests tests/protocol_tests.cpp)
  target_link_libraries(
    sourcetrace_protocol_tests
    PRIVATE sourcetrace_protocol Catch2::Catch2WithMain sourcetrace_build_options
  )
  target_compile_definitions(
    sourcetrace_protocol_tests
    PRIVATE SOURCETRACE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
  )
  catch_discover_tests(sourcetrace_protocol_tests)
endif()

if(SOURCETRACE_BUILD_FUZZER)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The protocol fuzzer requires Clang")
  endif()
  add_executable(sourcetrace_protocol_fuzz fuzz/protocol_fuzz.cpp)
  target_link_libraries(sourcetrace_protocol_fuzz PRIVATE sourcetrace_protocol)
  target_compile_options(
    sourcetrace_protocol_fuzz PRIVATE -fsanitize=fuzzer,address,undefined
  )
  target_link_options(
    sourcetrace_protocol_fuzz PRIVATE -fsanitize=fuzzer,address,undefined
  )
endif()
