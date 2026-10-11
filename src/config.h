#pragma once
#include <string>

namespace ev {

struct Config {
    std::string baseUrl;
    std::string apiKey;
    std::string logDir = "Logs";
    std::string imageDir;
};

// Reads config.json (first from the current directory, then next to the exe).
// Throws std::runtime_error with a clear message.
Config loadConfig();

}