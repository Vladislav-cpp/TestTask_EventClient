#include "api.h"
#include <iostream>
#include <string>
#include <variant>
#include <vector>
#include <nlohmann/json.hpp>

using namespace ev;

// ---- Відповіді сервера ----
static const std::string s_StatusBody =
    R"({"status":"online","api_version":"1.0","server_time":"t","candidate":"c"})";
static const std::string s_EventBody =
    R"({"event_id":42,"event_uid":"uid-1","event_code":7,"file_name":"a.jpg",)"
    R"("timestamp":"t","image_uploaded":false,"image_size":null})";
static const std::string s_ErrorBody =
    R"({"error":{"code":"validation_error","message":"bad code",)"
    R"("details":[{"field":"event_code","message":"too big"}]}})";

static const std::string s_Online = "online";
static const std::string s_EventUid = "uid-1";
static const std::string s_FileName = "a.jpg";
static const std::string s_Timestamp = "2026-10-05T12:00:00Z";
static const std::string s_NetworkDown = "network is down";

static const std::string s_KeyEventCode = "event_code";

static const int s_HttpOk = 200;
static const int s_HttpCreated = 201;
static const int s_HttpInvalid = 422;
static const int s_HttpTooManyRequests = 429;
static const int s_EventCode = 7;
static const long long s_EventId = 42;

static int s_Failures = 0;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            ++s_Failures; \
            std::cout << "  check failed (line " << __LINE__ << "): " #condition "\n"; \
        } \
    } while (false)

static HttpResponse makeResponse(int status, const std::string& body, int retryAfter = 0) {
    HttpResponse response;
    response.status = status;
    response.retryAfter = retryAfter;
    response.body.assign(body.begin(), body.end());
    return response;
}

// Returns pre-configured steps one by one: a response or a network failure. Remembers requests.
class FakeClient : public IHttpClient {
public:
    HttpResponse request(const HttpRequest& requestData) override {
        requests.push_back(requestData);
        const auto& step = script.at(requests.size() - 1);

        if (std::holds_alternative<NetworkError>(step)) {
            throw std::get<NetworkError>(step);
        }
        return std::get<HttpResponse>(step);
    }

    std::vector<std::variant<HttpResponse, NetworkError>> script;
    std::vector<HttpRequest> requests;
};


class FakeListener : public ApiListener {
public:
    void onWait(int seconds) override {
        waits.push_back(seconds);
    }

    std::vector<int> waits;
};

// ---- Tests ----

// 429: The API waits for the number of seconds specified in Retry-After and retries the request.
static void testRateLimitWaitsRetryAfter() {
    FakeClient client;
    client.script.push_back(makeResponse(s_HttpTooManyRequests, "", 1));
    client.script.push_back(makeResponse(s_HttpOk, s_StatusBody));
    FakeListener listener;
    Api api(client, listener);

    const ServerStatus status = api.getStatus();

    CHECK(status.status == s_Online);
    CHECK(client.requests.size() == 2);
    CHECK(listener.waits == std::vector<int>{1});
}

// Network failure: the event is resent with the EXACT SAME body, and therefore the same event_uid.
static void testNetworkErrorRetriesWithSameUid() {
    FakeClient client;
    client.script.push_back(NetworkError(s_NetworkDown));
    client.script.push_back(makeResponse(s_HttpCreated, s_EventBody));
    FakeListener listener;
    Api api(client, listener);

    const NewEvent event{s_EventUid, s_EventCode, s_FileName, s_Timestamp};
    const Event created = api.SendEvent(event);

    CHECK(created.event_id == s_EventId);
    CHECK(client.requests.size() == 2);
    CHECK(client.requests[0].body == client.requests[1].body);

    const std::string body(client.requests[0].body.begin(), client.requests[0].body.end());
    CHECK(body.find(s_EventUid) != std::string::npos);
}

// Upload of an image after a network failure is NOT retried (the server might have already saved the file).
static void testUploadIsNotRetried() {
    FakeClient client;
    client.script.push_back(NetworkError(s_NetworkDown));
    FakeListener listener;
    Api api(client, listener);

    bool threw = false;
    try {
        api.uploadImage(s_EventId, s_FileName, {1, 2, 3});
    } catch (const NetworkError&) {
        threw = true;
    }

    CHECK(threw);
    CHECK(client.requests.size() == 1);
    CHECK(listener.waits.empty());
}

// event_code must be an integer in JSON: the server rejects "7" and 7.0.
// if event_code in NewEvent changes to a different type, this test should fail.
static void testEventCodeIsJsonInteger() {
    FakeClient client;
    client.script.push_back(makeResponse(s_HttpCreated, s_EventBody));
    FakeListener listener;
    Api api(client, listener);

    api.SendEvent(NewEvent{s_EventUid, s_EventCode, s_FileName, s_Timestamp});

    const std::vector<unsigned char>& sent = client.requests.at(0).body;
    const nlohmann::json body = nlohmann::json::parse(std::string(sent.begin(), sent.end()));
    const nlohmann::json& code = body.at(s_KeyEventCode);

    CHECK(code.is_number_integer());   // 7
    CHECK(!code.is_number_float());    // not 7.0
    CHECK(!code.is_string());          // not "7"
    CHECK(code.get<int>() == s_EventCode);
}

// Server error: the text contains the HTTP code, code, message, and details.
static void testServerErrorText() {
    FakeClient client;
    client.script.push_back(makeResponse(s_HttpInvalid, s_ErrorBody));
    FakeListener listener;
    Api api(client, listener);

    std::string text;
    try {
        api.getStatus();
    } catch (const ApiError& e) {
        text = e.what();
    }

    CHECK(text.find("422") != std::string::npos);
    CHECK(text.find("validation_error") != std::string::npos);
    CHECK(text.find("bad code") != std::string::npos);
    CHECK(text.find("event_code") != std::string::npos);
    CHECK(text.find("too big") != std::string::npos);
}

// Multipart: boundary matches in header and body, filename = file_name, file bytes unchanged.
static void testUploadMultipartBody() {
    FakeClient client;
    client.script.push_back(makeResponse(s_HttpOk, s_EventBody));
    FakeListener listener;
    Api api(client, listener);

    const std::vector<unsigned char> image = {0xFF, 0xD8, 0xFF, 0x00, 0x0D, 0x0A, 0x7F};
    api.uploadImage(s_EventId, s_FileName, image);

    const HttpRequest& sent = client.requests.at(0);
    const std::string prefix = "multipart/form-data; boundary=";
    CHECK(sent.contentType.starts_with(prefix));

    const std::string boundary = sent.contentType.substr(prefix.size());
    const std::string body(sent.body.begin(), sent.body.end());

    CHECK(sent.path == "/events/42/image");
    CHECK(body.starts_with("--" + boundary + "\r\n"));
    CHECK(body.find("name=\"file\"; filename=\"a.jpg\"\r\n") != std::string::npos);
    CHECK(body.find("Content-Type: image/jpeg\r\n\r\n") != std::string::npos);
    CHECK(body.find(std::string(image.begin(), image.end())) != std::string::npos);
    CHECK(body.ends_with("\r\n--" + boundary + "--\r\n"));
}

static void run(const char* name, void (*test)()) {
    const int before = s_Failures;
    try {
        test();
    } catch (const std::exception& e) {
        ++s_Failures;
        std::cout << "  unexpected exception: " << e.what() << "\n";
    }
    std::cout << (s_Failures == before ? "[ ok ]   " : "[FAILED] ") << name << "\n";
}

int main() {
    run("429 waits Retry-After and retries", testRateLimitWaitsRetryAfter);
    run("network error retries with the same event_uid", testNetworkErrorRetriesWithSameUid);
    run("image upload is not retried", testUploadIsNotRetried);
    run("server error text has code, message, details", testServerErrorText);
    run("event_code is sent as a JSON integer", testEventCodeIsJsonInteger);
    run("multipart body is well formed", testUploadMultipartBody);

    std::cout << (s_Failures == 0 ? "All tests passed\n" : "Some tests failed\n");
    return s_Failures == 0 ? 0 : 1;
}