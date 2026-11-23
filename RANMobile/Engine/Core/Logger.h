#pragma once

#include <string>

namespace RANMobile {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    static void Log(LogLevel level, const std::string& tag, const std::string& message);
    static void Debug(const std::string& tag, const std::string& message);
    static void Info(const std::string& tag, const std::string& message);
    static void Warn(const std::string& tag, const std::string& message);
    static void Error(const std::string& tag, const std::string& message);
};

} // namespace RANMobile
