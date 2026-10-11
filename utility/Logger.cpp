#include "Logger.h"

#include <chrono>
#include <filesystem>
#include <format>

namespace fs = std::filesystem;

namespace ev {

static constexpr char s_FileName[]     = "eventclient_{:%F}.log";   // один файл на день (UTC)
static constexpr char s_LineFormat[]   = "{:%F %T}Z {:<9} {}";      // 2026-10-11 14:03:25Z [INFO]    текст
static constexpr char s_SessionStart[] = "--- session started ---";

static const std::string s_LevelInfo    = "[INFO]";
static const std::string s_LevelWarning = "[WARNING]";
static const std::string s_LevelError   = "[ERROR]";

static const char s_NewLine = '\n';

std::ofstream Logger::s_File;

static std::chrono::sys_seconds nowSeconds() {
    return std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
}

bool Logger::init(const std::string& directory) {
    std::error_code ec;
    fs::create_directories(directory, ec);
    if (ec) {
        return false;
    }

    s_File.open(fs::path(directory) / std::format(s_FileName, nowSeconds()), std::ios::app);
    if (!s_File.is_open()) {
        return false;
    }

    write(s_LevelInfo, s_SessionStart);   // межа між запусками в одному файлі
    return true;
}

void Logger::info(const std::string& text) {
    write(s_LevelInfo, text);
}

void Logger::warning(const std::string& text) {
    write(s_LevelWarning, text);
}

void Logger::error(const std::string& text) {
    write(s_LevelError, text);
}

void Logger::write(const std::string& level, const std::string& text) {
    if (!s_File.is_open()) {
        return;
    }

    s_File << std::format(s_LineFormat, nowSeconds(), level, text) << s_NewLine;
    s_File.flush();   // запис не втрачається при аварійному завершенні
}

}