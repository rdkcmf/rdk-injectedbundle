#ifndef RDK_LOGGER_H
#define RDK_LOGGER_H

namespace RDK
{

/**
 * Logging level with an increasing order of refinement
 * (TRACE_LEVEL = Finest logging).
 * It is essental to start with 0 and increase w/o gaps as the value
 * can be used for indexing in a mapping table.
 */
enum LogLevel {FATAL_LEVEL = 0, ERROR_LEVEL, WARNING_LEVEL, INFO_LEVEL, VERBOSE_LEVEL, TRACE_LEVEL};

/**
 * @brief Init logging
 * Should be called once per program run before calling log-functions
 */
void logger_init();

/**
 * @brief Log a message
 * The function is defined by logging backend.
 * Currently 2 variants are supported: rdk_logger (USE_RDK_LOGGER),
 *                                     stdout(default)
 */
void log(LogLevel level,
    const char* func,
    const char* file,
    int line,
    int threadID,
    const char* format, ...);

#define _LOG(LEVEL, FORMAT, ...)          \
    RDK::log(LEVEL,                       \
         __func__, __FILE__, __LINE__, 0, \
         FORMAT,                          \
         ##__VA_ARGS__)

#define RDKLOG_TRACE(FMT, ...)   _LOG(RDK::TRACE_LEVEL, FMT, ##__VA_ARGS__)
#define RDKLOG_VERBOSE(FMT, ...) _LOG(RDK::VERBOSE_LEVEL, FMT, ##__VA_ARGS__)
#define RDKLOG_INFO(FMT, ...)    _LOG(RDK::INFO_LEVEL, FMT, ##__VA_ARGS__)
#define RDKLOG_WARNING(FMT, ...) _LOG(RDK::WARNING_LEVEL, FMT, ##__VA_ARGS__)
#define RDKLOG_ERROR(FMT, ...)   _LOG(RDK::ERROR_LEVEL, FMT, ##__VA_ARGS__)
#define RDKLOG_FATAL(FMT, ...)   _LOG(RDK::FATAL_LEVEL, FMT, ##__VA_ARGS__)

} // namespace RDK

#endif  // RDK_LOGGER_H