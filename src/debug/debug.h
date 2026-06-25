#ifndef DEBUG_H
#define DEBUG_H

// Debug utilities: logging, assertions, profiling hooks.

#ifdef NDEBUG
  #define DEBUG_ASSERT(x) (void)(x)
  #define DEBUG_LOG(fmt, ...) (void)0
#else
  #define DEBUG_ASSERT(x) \
    do { \
      if (!(x)) { \
        __builtin_debugbreak(); \
      } \
    } while (0)
  #define DEBUG_LOG(fmt, ...) \
    fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif

#endif
