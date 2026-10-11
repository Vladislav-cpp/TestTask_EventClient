#include "application.h"
#include "util.h"
#include "Logger.h"
#include <filesystem>
#include <random>
#include <format>

namespace fs = std::filesystem;

namespace ev {
static constexpr char s_ErrEventNotPending[] = "Event {} not found among events without an image (wrong id or the image is already uploaded).";

static const std::string s_UserAgent = "EventClient/1.0";
static const int s_MinEventCode = 1;
static const int s_MaxEventCode = 10;
static const std::size_t s_MaxImageBytes = 5 * 1024 * 1024;

static constexpr char s_LogUploadFailed[] = "upload pending: event {} failed: {}";
static const std::string s_LogExit        = "client exit";

static std::mt19937 s_Generator{ std::random_device{}() };

Application::Application(Config config)
    : m_Config(std::move(config)),
      m_Http(m_Config.baseUrl, m_Config.apiKey, s_UserAgent),
      m_Api(m_Http, m_View) {
}

int Application::run() {

    for (;;) {
        m_View.showMenu();
        const MenuItem item = m_View.ReadMenuItem();

        try {
            switch (item) {
                case MenuItem::Empty:
                    
                    break;
                case MenuItem::Exit:

                    Logger::info(s_LogExit);
                    return 0;
                case MenuItem::Status:

                    doStatus();
                    break;
                case MenuItem::Send:

                    doSendEvent();
                    break;
                case MenuItem::Upload:

                    doUploadImage();
                    break;
                case MenuItem::List:

                    doListEvents();
                    break;
                case MenuItem::UploadPending:

                    doUploadPending();
                    break;
            }

        } catch (const std::exception& e) {   // ApiError, NetworkError, file errors
            
            Logger::error(e.what());
            m_View.showError(e.what());
        }
    }
}

void Application::doStatus() {
    try {
        m_View.showStatus(m_Api.getStatus());
    } catch (const NetworkError& e) {
        Logger::error(e.what());
        m_View.showOffline(e.what());
    }
}
void Application::doSendEvent() {

    NewEvent event;
    event.event_uid = generateEventUid();
    event.event_code = std::uniform_int_distribution<int>(s_MinEventCode, s_MaxEventCode)(s_Generator);
    event.file_name = pickRandomImage(m_Config.imageDir, s_MaxImageBytes);
    event.timestamp = utcNowIso();

    const Event created = m_Api.SendEvent(event);
    m_View.showEvent(created);
}

Event Application::uploadFile(long long eventId, const fs::path& file) {

    return m_Api.uploadImage(eventId, file.filename().string(), readFile(file, s_MaxImageBytes));
}

void Application::doUploadImage() {

    const auto id = m_View.ReadEventId();
    if (!id) {
        return;
    }

    for (const Event& event : m_Api.listEvents(true)) {

        if (event.event_id == *id) {

            // fileNameOf: the name from the server must not escape outside the directory.
            const fs::path file = fs::path(m_Config.imageDir) / fileNameOf(event.file_name);

            m_View.showEvent(uploadFile(event.event_id, file));
            return;
        }
    }

    throw std::runtime_error(std::format(s_ErrEventNotPending, *id));
}

void Application::doListEvents() {

    m_View.showEvents( m_Api.listEvents() );
}

void Application::doUploadPending() {
    const std::string dir = m_Config.imageDir;

    if( dir.empty() ) {
        return;
    }

    int uploaded = 0;
    int skipped = 0;
    int failed = 0;

    for( const Event& event : m_Api.listEvents(true) ) {

        // filename(): the name from the server must not escape outside the directory.
        const std::string name = fileNameOf(event.file_name);
        const fs::path file = fs::path(dir) / fs::path(name);

        std::error_code ec;
        if (!fs::is_regular_file(file, ec)) {
            m_View.showSkipped(event.event_id, name);
            ++skipped;
            continue;
        }

        try {

            m_View.showUploading(event.event_id, name);
            uploadFile(event.event_id, file);
            ++uploaded;
        } catch (const std::exception& e) {

			Logger::error(std::format(s_LogUploadFailed, event.event_id, e.what()));
            m_View.showFailed(event.event_id, e.what());
            ++failed;
        }
    }

    m_View.showPendingSummary(uploaded, skipped, failed);
}

}