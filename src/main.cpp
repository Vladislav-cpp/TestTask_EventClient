#include "application.h"
#include "Logger.h"
#include <windows.h>
#include <iostream>

static constexpr char s_LogStarted[] = "client started, server: {}";
static const std::string s_LogDisabled = "Warning: log folder is not available, logging is disabled.";


int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    try {

        const ev::Config config = ev::loadConfig();

        if (!ev::Logger::init(config.logDir)) {
            std::cerr << s_LogDisabled << "\n";
        }
        ev::Logger::info(std::format(s_LogStarted, config.baseUrl));


        ev::Application app(config);
        return app.run();
    } catch (const std::exception& e) {

        std::cerr << "Startup error: " << e.what() << "\n";
        return 1;
    } catch (...) {

        std::cerr << "Startup error: unknown exception\n";
        return 1;
    }
}