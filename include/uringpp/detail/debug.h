#pragma once

#include <cstdio>

#ifndef SOURCE_PATH_LENGTH
#define SOURCE_PATH_LENGTH 0
#endif

#define __FILENAME__ (&__FILE__[SOURCE_PATH_LENGTH])

namespace uringpp {

enum class LogLevel {
  TRACE,
  DEBUG,
  INFO,
  WARN,
  ERROR,
};
}

constexpr static inline uringpp::LogLevel uringpp_log_level =
    uringpp::LogLevel::DEBUG;

#define URINGPP_LOG_TRACE(msg, ...)                                            \
  do {                                                                         \
    if (uringpp_log_level > uringpp::LogLevel::TRACE)                          \
      break;                                                                   \
    printf("[TRACE] [%s:%d] " msg "\n", __FILENAME__,                          \
           __LINE__ __VA_OPT__(, ) __VA_ARGS__);                               \
  } while (0)

#define URINGPP_LOG_DEBUG(msg, ...)                                            \
  do {                                                                         \
    if (uringpp_log_level > uringpp::LogLevel::DEBUG)                          \
      break;                                                                   \
    printf("[DEBUG] [%s:%d] " msg "\n", __FILENAME__,                          \
           __LINE__ __VA_OPT__(, ) __VA_ARGS__);                               \
  } while (0)

#define URINGPP_LOG_INFO(msg, ...)                                             \
  do {                                                                         \
    if (uringpp_log_level > uringpp::LogLevel::INFO)                           \
      break;                                                                   \
    printf("[INFO ] [%s:%d] " msg "\n", __FILENAME__,                          \
           __LINE__ __VA_OPT__(, ) __VA_ARGS__);                               \
  } while (0)

#define URINGPP_LOG_ERROR(msg, ...)                                            \
  do {                                                                         \
    printf("[ERROR] [%s:%d] " msg "\n", __FILENAME__,                          \
           __LINE__ __VA_OPT__(, ) __VA_ARGS__);                               \
  } while (0)
