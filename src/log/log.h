#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#define LOG_LEVEL_DEBUG 10
#define LOG_LEVEL_INFO 20
#define LOG_LEVEL_WARN 30
#define LOG_LEVEL_ERROR 40

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/* safe readable version of errno */
#define clean_errno() (errno == 0 ? "None" : strerror(errno))

#if defined (LOG_NO_COLORS) || defined (_WIN32)
  #define log_error(M, ...) fprintf(stderr, "ERR   " M " at %s (%s:%d) errno:%s\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__, clean_errno())
  #define log_warn(M, ...)  fprintf(stderr, "WARN  " M " at %s (%s:%d) errno:%s\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__, clean_errno())
  #define log_info(M, ...)  fprintf(stderr, "INFO  " M " at %s (%s:%d)\n", ##__VA_ARGS__, __func__, __FILENAME__, __LINE__)
  #define log_debug(M, ...) fprintf(stderr, "DEBUG " M " at %s (%s:%d)\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__)
#else
  #define log_error(M, ...) fprintf(stderr, "\33[31mERR\33[39m   " M "  \33[90m at %s (%s:%d) \33[94merrno: %s\33[39m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__, clean_errno())
  #define log_warn(M, ...)  fprintf(stderr, "\33[91mWARN\33[39m  " M "  \33[90m at %s (%s:%d) \33[94merrno: %s\33[39m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__, clean_errno())
  #define log_info(M, ...)  fprintf(stderr, "\33[32mINFO\33[39m  " M "  \33[90m at %s (%s:%d) \33[39m\n", ##__VA_ARGS__, __func__, __FILENAME__, __LINE__)
  #define log_debug(M, ...) fprintf(stderr, "\33[34mDEBUG\33[39m " M "  \33[90m at %s (%s:%d) \33[39m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__)
#endif /* NOCOLORS */

#if LOG_LEVEL > LOG_LEVEL_ERROR
#undef log_error
#define log_error(M, ...) do { if (0) fprintf(stderr, "\33[31mERR\33[39m   " M "  \33[90m at %s (%s:%d) \33[94merrno: %s\33[39m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__, clean_errno()); } while(0)
#endif

#if LOG_LEVEL > LOG_LEVEL_WARN
#undef log_warn
#define log_warn(M, ...)  do { if (0) fprintf(stderr, "\33[91mWARN\33[39m  " M "  \33[90m at %s (%s:%d) \33[94merrno: %s\33[39m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__, clean_errno()); } while (0)
#endif

#if LOG_LEVEL > LOG_LEVEL_INFO
#undef log_info
#define log_info(M, ...)  do { if (0) fprintf(stderr, "\33[32mINFO\33[39m  " M "  \33[90m at %s (%s:%d) \33[39m\n", ##__VA_ARGS__, __func__, __FILENAME__, __LINE__); } while (0)
#endif

#if LOG_LEVEL > LOG_LEVEL_DEBUG
#undef log_debug
#define log_debug(M, ...) do { if (0) fprintf(stderr, "\33[34mDEBUG\33[39m " M "  \33[90m at %s (%s:%d) \33[39m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__); } while (0)
#endif

#endif //LOG_H
