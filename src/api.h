#pragma once
#include "HttpTypes.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace ev {


struct ServerStatus {
    std::string status;
    std::string api_version;
    std::string server_time;
};

struct NewEvent {
    std::string event_uid;     // Generated once, retries do not change it
    int event_code = 0;        // 1..10
    std::string file_name;     // Base file name only (no path)
    std::string timestamp;     // UTC ISO-8601
};

struct Event {
    long long event_id = 0;
    std::string event_uid;
    int event_code = 0;
    std::string file_name;
    std::string timestamp;
    bool image_uploaded = false;
};

// Server responded with an error. what() = "HTTP 422 validation_error: message" and details lines.
class ApiError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Server notifies through this interface about waiting before retry (429 or network failure).
class ApiListener {
public:
    virtual ~ApiListener() = default;
    virtual void onWait(int seconds) = 0;
};

class Api {
public:
    Api(IHttpClient& http, ApiListener& listener);

    ServerStatus getStatus();
    Event SendEvent(const NewEvent& event);
    Event uploadImage(long long eventId, const std::string& fileName,
                      const std::vector<unsigned char>& data);
    std::vector<Event> listEvents(bool pendingOnly = false);

private:
    // 429: waits for Retry-After and retries. Network failure: retries if safeToRepeat.
    HttpResponse send(const HttpRequest& request, bool safeToRepeat);
    void wait(int seconds);

    IHttpClient& m_Http;
    ApiListener& m_Listener;
};

}