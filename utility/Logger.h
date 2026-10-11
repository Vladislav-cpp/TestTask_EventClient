#pragma once
#include <fstream>
#include <string>

namespace ev {

// Глобальний файловий логер: Logger::info("...") можна викликати з будь-якого місця.
// Поки не викликано init (наприклад, у тестах), усі виклики нічого не роблять.
// У лог не пишемо API-ключ, заголовки запитів і вміст файлів.
class Logger {
public:
    // Створює теку directory і відкриває в ній eventclient_<дата UTC>.log у режимі дописування.
    // false, якщо не вдалося: логування лишається вимкненим, програма працює далі.
    static bool init(const std::string& directory);

    static void info(const std::string& text);
    static void warning(const std::string& text);
    static void error(const std::string& text);

private:
    static void write(const std::string& level, const std::string& text);

    static std::ofstream s_File;
};

}