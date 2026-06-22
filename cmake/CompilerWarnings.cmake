# A small interface target carrying a modest warning set. Kept intentionally
# light to suit the "C with classes" style; tighten over time by adding flags
# here (e.g. -Wshadow, -Wconversion) rather than peppering targets.
add_library(mach_warnings INTERFACE)

if(MSVC)
  target_compile_options(mach_warnings INTERFACE /W4)
else()
  target_compile_options(mach_warnings INTERFACE -Wall -Wextra)
endif()
