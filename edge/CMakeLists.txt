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

# Product targets are introduced in the focused edge commits.
