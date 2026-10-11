#pragma once
#include "api.h"
#include "config.h"
#include "console_view.h"
#include "HttpClient.h"
#include <filesystem> 

namespace ev {

// Manages the flow: menu, loop, API calls. It does not print or read from the console; ConsoleView handles that.
class Application {
public:
    explicit Application(Config config);
    int run();

private:
    void doStatus();
    void doSendEvent();
    void doUploadImage();
    void doListEvents();
    void doUploadPending();

    Event uploadFile(long long eventId, const std::filesystem::path& file);

    Config m_Config;
    ConsoleView m_View;
    HttpClient m_Http;
    Api m_Api;
};

}