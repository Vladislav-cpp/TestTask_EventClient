#include "api.h"
#include "util.h"
#include "Logger.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>
#include <format>

namespace fs = std::filesystem;

namespace ev {

using json = nlohmann::json;

struct EventPage {
    int total = 0;
    std::vector<Event> items;
};

// JSON <-> структури: ключі беруться з імен полів.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( ServerStatus, status, api_version, server_time )
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( NewEvent, event_uid, event_code, file_name, timestamp )
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( Event, event_id, event_uid, event_code, file_name, timestamp, image_uploaded )
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE( EventPage, total, items )

// ---- Запити ----
static const std::string s_MethodGet    = "GET";
static constexpr char s_MethodPost[]   = "POST";
static const std::string s_PathStatus   = "/status";
static const std::string s_PathEvents   = "/events";
static constexpr char s_PathImage[]    = "/events/{}/image";

static constexpr char s_PathPage[]      = "/events?limit=100&offset={}";
static const std::string s_QueryPending = "&image_uploaded=false";

// ---- Тіло запиту ----
static const std::string s_TypeJson       = "application/json";
static constexpr char s_TypeMultipart[]  = "multipart/form-data; boundary=";
static const std::string s_BoundaryPrefix = "----EventClient";
static constexpr char s_PartHead[] = "--{}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"{}\"\r\nContent-Type: {}\r\n\r\n";
static constexpr char s_PartTail[] = "\r\n--{}--\r\n";
static const std::string s_ExtPng   = ".png";
static const std::string s_MimePng  = "image/png";
static const std::string s_MimeJpeg = "image/jpeg";

// ---- Розбір помилок ----
static const std::string s_KeyError     = "error";
static const std::string s_KeyCode      = "code";
static const std::string s_KeyMessage   = "message";
static const std::string s_KeyDetails   = "details";
static const std::string s_KeyField     = "field";

static constexpr char s_StatusOnly[]  = "HTTP {}";
static constexpr char s_ErrorLine[]   = "HTTP {} {}: {}";
static constexpr char s_DetailLine[]  = "\n  - {}: {}";
static const std::string s_BadResponse  = "Unexpected response: ";

// ---- Повтори ----
static const int s_HttpOkFirst         = 200;
static const int s_HttpOkLast          = 299;
static const int s_HttpTooManyRequests = 429;
static const int s_MaxAttempts         = 5;
static const int s_MinWaitSeconds      = 1;
static const int s_MaxWaitSeconds      = 120;
static const int s_NetworkDelayStep    = 2;


// ---- Лог ----
static constexpr char s_LogSendEvent[]     = "send event uid={} code={} file={}";
static constexpr char s_LogResponse[]      = "{} {} -> {}";
static constexpr char s_LogRateLimited[]   = "{} {} rate limited (429); next attempt {}/{} in {} s";
static constexpr char s_LogNetworkRetry[]  = "{} {} network error: {}; next attempt {}/{} in {} s";
static constexpr char s_LogNetworkFailed[] = "{} {} network error: {}; giving up";

static std::vector<unsigned char> toBytes(const std::string& text) {
    return std::vector<unsigned char>(text.begin(), text.end());
}

// {"error": {"code", "message", "details"}} -> текст для користувача.
static std::string errorText(const HttpResponse& response) {
    std::string text = std::format(s_StatusOnly, response.status);
    try {
        const json error = json::parse(response.body).at(s_KeyError);
        text = std::format(s_ErrorLine, response.status,
                          error.at(s_KeyCode).get<std::string>(),
                          error.at(s_KeyMessage).get<std::string>());
        for (const auto& detail : error.value(s_KeyDetails, json::array())) {
            text += std::format(s_DetailLine,
                               detail.at(s_KeyField).get<std::string>(),
                               detail.at(s_KeyMessage).get<std::string>());
        }
    } catch (const json::exception&) {
    }
    return text;
}

template <class T>
static T parseResponse(const HttpResponse& response) {

    if (response.status < s_HttpOkFirst || response.status > s_HttpOkLast) {
        throw ApiError(errorText(response));
    }
    try {

        return json::parse(response.body).get<T>();
    } catch (const json::exception& e) {

        throw ApiError(s_BadResponse + e.what());
    }
}

Api::Api(IHttpClient& http, ApiListener& listener) : m_Http(http), m_Listener(listener) {
}

void Api::wait(int seconds) {
    m_Listener.onWait(seconds);
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}

HttpResponse Api::send(const HttpRequest& request, bool safeToRepeat) {
    for (int attempt = 1;; ++attempt) {
        try {

            const HttpResponse response = m_Http.request(request);
            Logger::info(std::format(s_LogResponse, request.method, request.path, response.status));

            if (response.status != s_HttpTooManyRequests || attempt >= s_MaxAttempts) {
                return response;
            }
            const int seconds = std::clamp(response.retryAfter, s_MinWaitSeconds, s_MaxWaitSeconds);
            Logger::warning(std::format(s_LogRateLimited, request.method, request.path, attempt + 1, s_MaxAttempts, seconds));

            wait(std::clamp(response.retryAfter, s_MinWaitSeconds, s_MaxWaitSeconds));   // Retry-After
        } catch (const ev::NetworkError& e) {

            if (!safeToRepeat || attempt >= s_MaxAttempts) {
				Logger::error(std::format(s_LogNetworkFailed, request.method, request.path, e.what()));   
                throw;
            }

            const int seconds = attempt * s_NetworkDelayStep;
            Logger::warning(std::format(s_LogNetworkRetry, request.method, request.path, e.what(),
                            attempt + 1, s_MaxAttempts, seconds)); 

            wait(attempt * s_NetworkDelayStep);
        }
    }
}

ServerStatus Api::getStatus() {
    
	HttpRequest request{ s_MethodGet, s_PathStatus, {}, {} };
    auto response = send( request, true);

    return parseResponse<ServerStatus>( response );
}

Event Api::SendEvent(const NewEvent& event) {
    const std::string body = json(event).dump();

    Logger::info(std::format(s_LogSendEvent, event.event_uid, event.event_code, event.file_name));

    return parseResponse<Event>(send({s_MethodPost, s_PathEvents, s_TypeJson, toBytes(body)}, true));
}

Event Api::uploadImage(long long eventId, const std::string& fileName,
                       const std::vector<unsigned char>& data) {

    const std::string boundary = s_BoundaryPrefix + generateEventUid();
    const std::string mime = fs::path(fileName).extension().string() == s_ExtPng ? s_MimePng : s_MimeJpeg;
    const std::string tail = std::format(s_PartTail, boundary);

    std::vector<unsigned char> body = toBytes(std::format(s_PartHead, boundary, fileName, mime));
    body.insert(body.end(), data.begin(), data.end());
    body.insert(body.end(), tail.begin(), tail.end());


    return parseResponse<Event>(send({s_MethodPost, std::format(s_PathImage, eventId),
                                      s_TypeMultipart + boundary, std::move(body)}, false));
}

std::vector<Event> Api::listEvents(bool pendingOnly) {
    std::vector<Event> events;

    for (;;) {
        std::string path = std::format(s_PathPage, events.size());
        if (pendingOnly) {
            path += s_QueryPending;
        }

        const EventPage page = parseResponse<EventPage>(send({s_MethodGet, path, {}, {}}, true));
        events.insert(events.end(), page.items.begin(), page.items.end());

        if (page.items.empty() || events.size() >= static_cast<std::size_t>(page.total)) {
            return events;
        }
    }
}

}