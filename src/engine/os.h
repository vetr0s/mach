#ifndef OS_H
#define OS_H

// Platform abstraction layer.
// Abstracts away macOS/Linux/Windows specific details.

#if defined(__APPLE__)
  #define OS_MAC 1
#elif defined(__linux__)
  #define OS_LINUX 1
#elif defined(_WIN32) || defined(_WIN64)
  #define OS_WINDOWS 1
#endif

#endif
