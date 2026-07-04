#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace chimera {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void set_level(LogLevel level) { level_ = level; }

    void set_output(FILE* file) {
        std::lock_guard<std::mutex> lock(mutex_);
        file_ = file;
    }

    void log(LogLevel level, const char* file, int line, const char* fmt, ...) {
        if (level < level_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        if (!file_) return;

        const char* level_str = "";
        switch (level) {
            case LogLevel::Debug:   level_str = "DEBUG"; break;
            case LogLevel::Info:    level_str = "INFO"; break;
            case LogLevel::Warning: level_str = "WARN"; break;
            case LogLevel::Error:   level_str = "ERROR"; break;
        }

        fprintf(file_, "[%s] ", level_str);
        if (file) fprintf(file_, "%s:%d ", file, line);

        va_list args;
        va_start(args, fmt);
        vfprintf(file_, fmt, args);
        va_end(args);

        fprintf(file_, "\n");
        fflush(file_);
    }

private:
    Logger() : file_(stderr) {}
    LogLevel level_ = LogLevel::Info;
    FILE* file_;
    std::mutex mutex_;
};

} // namespace chimera

#define CHIMERA_LOG(level, ...) \
    chimera::Logger::instance().log(level, __FILE__, __LINE__, __VA_ARGS__)

#define CHIMERA_DEBUG(...) CHIMERA_LOG(chimera::LogLevel::Debug, __VA_ARGS__)
#define CHIMERA_INFO(...)  CHIMERA_LOG(chimera::LogLevel::Info, __VA_ARGS__)
#define CHIMERA_WARN(...)  CHIMERA_LOG(chimera::LogLevel::Warning, __VA_ARGS__)
#define CHIMERA_ERROR(...) CHIMERA_LOG(chimera::LogLevel::Error, __VA_ARGS__)
