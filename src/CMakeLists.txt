# Engine core, as a static library so future tools (editor, etc.) can link it.
add_library(mach STATIC
  mach/engine.cpp
)

# Public headers live under src/ and are included as <mach/...>.
target_include_directories(mach PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(mach
  PUBLIC  SDL3::SDL3
  PRIVATE mach_warnings
)

# The runnable sample / smoke test.
add_executable(mach_game main.cpp)
target_link_libraries(mach_game
  PRIVATE mach mach_warnings
)

# SDL3 is a shared library, so copy its runtime next to the executable so the
# binary runs straight from the build tree.
add_custom_command(TARGET mach_game POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:mach_game>
  VERBATIM
)
