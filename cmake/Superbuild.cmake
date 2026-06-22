# Superbuild orchestrator: builds SDL3 to a known per-platform install prefix,
# then builds the engine against it. Included from the root CMakeLists.txt when
# MACH_SUPERBUILD is ON.
include(ExternalProject)

# A stable, per-platform tag for the install location, e.g. "darwin-arm64",
# "linux-x86_64", "windows-amd64". Lower-cased for tidiness.
string(TOLOWER "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}" MACH_PLATFORM)
set(SDL_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/third_party/SDL/install/${MACH_PLATFORM}")

# Config to forward to the sub-builds. With multi-config generators the build
# type is chosen at build time, so only forward it when set (single-config).
set(MACH_FORWARD_ARGS
  "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
)
if(CMAKE_OSX_DEPLOYMENT_TARGET)
  list(APPEND MACH_FORWARD_ARGS
    "-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()
if(CMAKE_OSX_ARCHITECTURES)
  list(APPEND MACH_FORWARD_ARGS
    "-DCMAKE_OSX_ARCHITECTURES:STRING=${CMAKE_OSX_ARCHITECTURES}")
endif()

# --- SDL3: build shared, install to the known prefix --------------------------
ExternalProject_Add(SDL3
  SOURCE_DIR    "${CMAKE_SOURCE_DIR}/third_party/SDL"
  INSTALL_DIR   "${SDL_INSTALL_PREFIX}"
  CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX:PATH=${SDL_INSTALL_PREFIX}"
    -DSDL_SHARED:BOOL=ON
    -DSDL_STATIC:BOOL=OFF
    -DSDL_TEST_LIBRARY:BOOL=OFF
    -DSDL_TESTS:BOOL=OFF
    -DSDL_EXAMPLES:BOOL=OFF
    ${MACH_FORWARD_ARGS}
  BUILD_COMMAND   ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG>
  INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR> --config $<CONFIG>
)

# --- Engine: build against the SDL3 prefix ------------------------------------
ExternalProject_Add(mach-engine
  SOURCE_DIR  "${CMAKE_SOURCE_DIR}"
  BINARY_DIR  "${CMAKE_BINARY_DIR}/engine"
  DEPENDS     SDL3
  CMAKE_ARGS
    -DMACH_SUPERBUILD:BOOL=OFF
    "-DCMAKE_PREFIX_PATH:PATH=${SDL_INSTALL_PREFIX}"
    ${MACH_FORWARD_ARGS}
  BUILD_COMMAND   ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG>
  INSTALL_COMMAND ""
  # Always step into the engine sub-build so source edits rebuild without having
  # to reconfigure the superbuild.
  BUILD_ALWAYS ON
)
