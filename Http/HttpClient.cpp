#include "HttpClient.h"
#include "winhttp_helpers.h"
#include "util.h"

namespace ev {

// ---- Timeouts (ms) ----
static const int s_ResolveTimeout = 10'000;
static const int s_ConnectTimeout = 10'000;
static const int s_SendTimeout    = 30'000;
static const int s_ReceiveTimeout = 60'000;

// ---- Headers ----
static const std::string s_HeaderApiKey      = "X-API-Key: ";
static const std::string s_HeaderAccept      = "Accept: application/json";
static const std::string s_HeaderContentType = "Content-Type: ";
static const std::string s_HeaderEnd         = "\r\n";

// ---- Error texts ----
static const std::string s_ErrOpen        = "WinHttpOpen failed";
static const std::string s_ErrTimeouts    = "WinHttpSetTimeouts failed";
static const std::string s_ErrConnect     = "WinHttpConnect failed";
static const std::string s_ErrOpenRequest = "WinHttpOpenRequest failed";

void HttpClient::HandleCloser::operator()(void* handle) const noexcept {
    if (handle) {
        WinHttpCloseHandle(handle);
    }
}

HttpClient::HttpClient(
    const std::string& baseUrl,
    const std::string& apiKey,
    const std::string& userAgent) : m_ApiKey(apiKey) {

    const winhttp::UrlParts url = winhttp::parseUrl(baseUrl);
    m_BasePath = url.basePath;

    m_Session.reset(WinHttpOpen(
        utf8ToWide(userAgent).c_str(),
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    ));
    winhttp::ensure(m_Session != nullptr, s_ErrOpen);

    winhttp::ensure(WinHttpSetTimeouts(m_Session.get(), s_ResolveTimeout, s_ConnectTimeout,
                                       s_SendTimeout, s_ReceiveTimeout),
                    s_ErrTimeouts);

    m_Connection.reset(WinHttpConnect(m_Session.get(), url.host.c_str(), url.port, 0));
    winhttp::ensure(m_Connection != nullptr, s_ErrConnect);
}

HttpResponse HttpClient::request(const HttpRequest& requestData) {
    const Handle handle = openRequest(requestData);   // will close itself even if an exception is thrown below
    winhttp::sendAndReceive(handle.get(), buildHeaders(requestData), requestData.body);

    HttpResponse response;
    response.status = winhttp::readStatus(handle.get());
    response.retryAfter = winhttp::readRetryAfter(handle.get());
    response.body = winhttp::readBody(handle.get());

    return response;
}

HttpClient::Handle HttpClient::openRequest(const HttpRequest& requestData) const {
    const std::wstring method = utf8ToWide(requestData.method);
    const std::wstring path = m_BasePath + utf8ToWide(requestData.path);

    // WINHTTP_FLAG_SECURE = HTTPS. We do not touch WINHTTP_OPTION_SECURITY_FLAGS,
   // so certificate validation remains enabled.
    Handle handle(WinHttpOpenRequest(
        m_Connection.get(),
        method.c_str(),
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    ));

    winhttp::ensure(handle != nullptr, s_ErrOpenRequest);
    return handle;
}

std::wstring HttpClient::buildHeaders(const HttpRequest& requestData) const {
    std::string headers = s_HeaderApiKey + m_ApiKey + s_HeaderEnd + s_HeaderAccept + s_HeaderEnd;

    if (!requestData.contentType.empty()) {
        headers += s_HeaderContentType + requestData.contentType + s_HeaderEnd;
    }

    return utf8ToWide(headers);
}

}