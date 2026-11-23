#include "Logger.h"
#include <iostream>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace RANMobile {

void Logger::Log(LogLevel level, const std::string& tag, const std::string& message) {
#ifdef __ANDROID__
    int prio = ANDROID_LOG_INFO;
    switch (level) {
        case LogLevel::Debug:   prio = ANDROID_LOG_DEBUG; break;
        case LogLevel::Info:    prio = ANDROID_LOG_INFO;  break;
        case LogLevel::Warning: prio = ANDROID_LOG_WARN;  break;
        case LogLevel::Error:   prio = ANDROID_LOG_ERROR; break;
    }
    __android_log_print(prio, tag.c_str(), "%s", message.c_str());
#else
    const char* levelStr = "INFO";
    switch (level) {
        case LogLevel::Debug:   levelStr = "DEBUG"; break;
        case LogLevel::Info:    levelStr = "INFO";  break;
        case LogLevel::Warning: levelStr = "WARN";  break;
        case LogLevel::Error:   levelStr = "ERROR"; break;
    }
    std::cout << "[" << levelStr << "][" << tag << "] " << message << "\n";
#endif
}

void Logger::Debug(const std::string& tag, const std::string& message) { Log(LogLevel::Debug, tag, message); }
void Logger::Info(const std::string& tag, const std::string& message)  { Log(LogLevel::Info, tag, message); }
void Logger::Warn(const std::string& tag, const std::string& message)  { Log(LogLevel::Warning, tag, message); }
void Logger::Error(const std::string& tag, const std::string& message) { Log(LogLevel::Error, tag, message); }

} // namespace RANMobile
