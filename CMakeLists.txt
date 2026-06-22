cmake_minimum_required(VERSION 3.21)
project(mach VERSION 0.1.0 LANGUAGES CXX)

# This file is dual-mode (the standard CMake "superbuild" idiom):
#
#   MACH_SUPERBUILD=ON  (default) -> build the SDL3 dependency to a known
#                                    install prefix, then re-invoke CMake on
#                                    this same source tree with the flag OFF to
#                                    build the engine against that prefix.
#   MACH_SUPERBUILD=OFF           -> build the actual engine; SDL3 is found via
#                                    find_package() from the prefix above.
#
# Keeping SDL3 in its own ExternalProject means it builds once and engine edits
# never trigger an SDL3 rebuild.
option(MACH_SUPERBUILD "Build the SDL3 dependency, then the engine" ON)
if(MACH_SUPERBUILD)
  include(cmake/Superbuild.cmake)
  return()
endif()

# ---------------------------------------------------------------------------
# Engine build (MACH_SUPERBUILD=OFF)
# ---------------------------------------------------------------------------
# C++17 is the baseline: enough modern C++ to reach for when wanted, while a
# "C with classes" style stays perfectly natural. No compiler extensions.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single, predictable place for the runnable binary across generators.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(SDL3 CONFIG REQUIRED)

include(cmake/CompilerWarnings.cmake)

add_subdirectory(src)
