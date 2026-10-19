#pragma once

// host-side stand-in for the Android NDK logging header

#include <cstdio>

enum {
  ANDROID_LOG_DEBUG = 3,
  ANDROID_LOG_INFO = 4,
  ANDROID_LOG_WARN = 5,
  ANDROID_LOG_ERROR = 6,
};

#define __android_log_print(prio, tag, ...) \
  (std::fprintf(stderr, "[%s] ", tag), std::fprintf(stderr, __VA_ARGS__), std::fprintf(stderr, "\n"))
