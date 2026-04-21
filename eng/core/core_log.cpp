// core_log.cpp                                                       -*-C++-*-
#include <core/core_log.h>

// std
#include <chrono>
#include <iomanip>
#include <iostream>

namespace eng::core {

std::mutex Logger::s_logMutex;

constexpr auto COLOR_RESET = "\033[0m";
constexpr auto COLOR_TRACE = "\033[90m";    // Gris
constexpr auto COLOR_INFO  = "\033[32m";    // Vert
constexpr auto COLOR_WARN  = "\033[33m";    // Jaune
constexpr auto COLOR_ERROR = "\033[31m";    // Rouge
constexpr auto COLOR_FATAL = "\033[1;31m";  // Rouge Gras

constexpr const char* getFileName(const char* path)
{
    const char* file = strrchr(path, '/');
    if (!file)
        file = strrchr(path, '\\');
    return file ? file + 1 : path;
}

void Logger::output(const LogLevel     level,
                    const char*        file,
                    const int          line,
                    const std::string& message)
{
    std::lock_guard lock(s_logMutex);

    const auto now        = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm    bt{};
    localtime_r(&time_t_now, &bt);

    const char* colorCode = COLOR_RESET;
    auto        levelStr  = "";

    switch (level) {
    case LogLevel::TRACE:
        colorCode = COLOR_TRACE;
        levelStr  = "[TRACE]";
        break;
    case LogLevel::INFO:
        colorCode = COLOR_INFO;
        levelStr  = "[INFO] ";
        break;
    case LogLevel::WARN:
        colorCode = COLOR_WARN;
        levelStr  = "[WARN] ";
        break;
    case LogLevel::ERR:
        colorCode = COLOR_ERROR;
        levelStr  = "[ERROR]";
        break;
    case LogLevel::FATAL:
        colorCode = COLOR_FATAL;
        levelStr  = "[FATAL]";
        break;
    }

    std::cout << colorCode << "[" << std::put_time(&bt, "%H:%M:%S") << "] "
              << levelStr << " "
              << "[" << getFileName(file) << ":" << line << "] " << message
              << COLOR_RESET << "\n";
}

}  // close package namespace