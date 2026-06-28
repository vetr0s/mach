#ifndef DEBUG_H
#define DEBUG_H

// Debug utilities: leveled logging and assertions.
//
// Logging levels:
//   LOG_INFO  - lifecycle / high-level events (always compiled in)
//   LOG_ERROR - failures and recoverable errors (always compiled in)
//   LOG_DEBUG - verbose per-frame / per-event tracing (debug builds only)
//
// All output goes to stderr with a level tag so logs are greppable.

#include <stdio.h>

#define LOG_INFO(fmt, ...) \
  fprintf(stderr, "[INFO]  " fmt "\n", ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
  fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

#ifdef NDEBUG
  #define DEBUG_ASSERT(x) (void)(x)
  #define LOG_DEBUG(fmt, ...) (void)0
#else
  #define DEBUG_ASSERT(x) \
    do { \
      if (!(x)) { \
        LOG_ERROR("assertion failed: %s (%s:%d)", #x, __FILE__, __LINE__); \
        __builtin_debugbreak(); \
      } \
    } while (0)
  #define LOG_DEBUG(fmt, ...) \
    fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif

#endif
