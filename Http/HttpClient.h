#pragma once

#include "HttpTypes.h"

#include <memory>
#include <string>

namespace ev {

class HttpClient final : public IHttpClient {
public:
    HttpClient(
        const std::string& baseUrl,
        const std::string& apiKey,
        const std::string& userAgent
    );

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    HttpResponse request(const HttpRequest& requestData) override;

private:
    struct HandleCloser {
        void operator()(void* handle) const noexcept;   // WinHttpCloseHandle
    };
    using Handle = std::unique_ptr<void, HandleCloser>;

    Handle openRequest(const HttpRequest& requestData) const;
    std::wstring buildHeaders(const HttpRequest& requestData) const;

    // Order matters: members are destroyed in reverse order,
    // so the connection is closed before the session.
    Handle m_Session;
    Handle m_Connection;
    std::wstring m_BasePath;
    std::string m_ApiKey;
};

}