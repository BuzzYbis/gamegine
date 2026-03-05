// log.h                                                              -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_LOG_H
#define INCLUDED_ENGINE_CORE_LOG_H

// std
#include <mutex>
#include <sstream>
#include <string>

namespace engine::core {

enum class LogLevel { TRACE, INFO, WARN, ERR, FATAL };

class Logger {
  public:
    template <typename... Args>
    static void
    log(const LogLevel level, const char* file, const int line, Args&&... args)
    {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));

        output(level, file, line, oss.str());
    }

  private:
    static void       output(LogLevel           level,
                             const char*        file,
                             int                line,
                             const std::string& message);
    static std::mutex s_logMutex;
};

}  // close engine::core namespace

#define LOG_TRACE(...)                                                        \
    engine::core::Logger::log(engine::core::LogLevel::TRACE,                  \
                              __FILE__,                                       \
                              __LINE__,                                       \
                              __VA_ARGS__)
#define LOG_INFO(...)                                                         \
    engine::core::Logger::log(engine::core::LogLevel::INFO,                   \
                              __FILE__,                                       \
                              __LINE__,                                       \
                              __VA_ARGS__)
#define LOG_WARN(...)                                                         \
    engine::core::Logger::log(engine::core::LogLevel::WARN,                   \
                              __FILE__,                                       \
                              __LINE__,                                       \
                              __VA_ARGS__)
#define LOG_ERROR(...)                                                        \
    engine::core::Logger::log(engine::core::LogLevel::ERR,                    \
                              __FILE__,                                       \
                              __LINE__,                                       \
                              __VA_ARGS__)
#define LOG_FATAL(...)                                                        \
    engine::core::Logger::log(engine::core::LogLevel::FATAL,                  \
                              __FILE__,                                       \
                              __LINE__,                                       \
                              __VA_ARGS__)

#endif  // INCLUDED_ENGINE_CORE_LOG_H