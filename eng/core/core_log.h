// core_log.h                                                         -*-C++-*-
#ifndef INCLUDED_ENG_CORE_LOG_H
#define INCLUDED_ENG_CORE_LOG_H

//@PURPOSE: Provide a centralized thread-safe logging utility.
//
//@CLASSES:
//  eng::core::LogLevel: Enumeration of logging severity levels.
//  eng::core::Logger: Utility class for message output and formatting.
//
//@DESCRIPTION: This component provides a global logging facility for the
// engine. It supports multiple severity levels (TRACE to FATAL) and uses a
// thread-safe mechanism to output messages to standard output with file
// and line information. Convenience macros (LOG_INFO, etc.) are provided
// for easy use throughout the codebase.

// std
#include <mutex>
#include <sstream>
#include <string>

namespace eng::core {

/// Enumerates the severity levels for log messages.
enum class LogLevel { TRACE, INFO, WARN, ERR, FATAL };

// ============
// class Logger
// ============

/// This utility class provides static methods for formatting and emitting
/// log messages.
class Logger {
  public:
    // CLASS METHODS

    /// Format and output a log message with the specified 'level' originating
    /// from the specified 'file' and 'line', using the specified variadic
    /// 'args' as the message content.
    template <typename... Args>
    static void
    log(const LogLevel level, const char* file, const int line, Args&&... args)
    {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));

        output(level, file, line, oss.str());
    }

  private:
    // PRIVATE CLASS METHODS
    static void       output(LogLevel           level,
                             const char*        file,
                             int                line,
                             const std::string& message);
    static std::mutex s_logMutex;
};

}  // close package namespace

#define LOG_TRACE(...)                                                        \
    eng::core::Logger::log(eng::core::LogLevel::TRACE,                        \
                           __FILE__,                                          \
                           __LINE__,                                          \
                           __VA_ARGS__)
#define LOG_INFO(...)                                                         \
    eng::core::Logger::log(eng::core::LogLevel::INFO,                         \
                           __FILE__,                                          \
                           __LINE__,                                          \
                           __VA_ARGS__)
#define LOG_WARN(...)                                                         \
    eng::core::Logger::log(eng::core::LogLevel::WARN,                         \
                           __FILE__,                                          \
                           __LINE__,                                          \
                           __VA_ARGS__)
#define LOG_ERROR(...)                                                        \
    eng::core::Logger::log(eng::core::LogLevel::ERR,                          \
                           __FILE__,                                          \
                           __LINE__,                                          \
                           __VA_ARGS__)
#define LOG_FATAL(...)                                                        \
    eng::core::Logger::log(eng::core::LogLevel::FATAL,                        \
                           __FILE__,                                          \
                           __LINE__,                                          \
                           __VA_ARGS__)

#endif  // INCLUDED_ENG_CORE_LOG_H