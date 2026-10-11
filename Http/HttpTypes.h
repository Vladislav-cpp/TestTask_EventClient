#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace ev {

// Network-level failure. retryable = false if retrying will not change anything (e.g., certificate validation failed).
class NetworkError : public std::runtime_error {
public:
    explicit NetworkError(const std::string& message, bool retryable = true)
        : std::runtime_error(message), m_Retryable(retryable) {
    }

    bool retryable() const {
        return m_Retryable;
    }

private:
    bool m_Retryable;
};

struct HttpRequest {
    std::string method;
    std::string path;                    // relative to the base URL
    std::string contentType;             // empty = header is not added
    std::vector<unsigned char> body;
};

struct HttpResponse {
    int status = 0;
    int retryAfter = 0;                  // seconds from Retry-After, 0 = no header
    std::vector<unsigned char> body;
};

// Transport interface: a fake HttpClient is substituted for the real one in tests.
class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse request(const HttpRequest& requestData) = 0;
};

}