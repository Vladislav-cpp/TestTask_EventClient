#include "winhttp_helpers.h"
#include "HttpTypes.h"
#include "util.h"

#include <format>
#include <stdexcept>

namespace ev::winhttp {

// ---- Limits ----
static const std::size_t s_MaxResponseBytes = 10 * 1024 * 1024;
static const DWORD s_RetryAfterChars = 32;

static const wchar_t s_PathSlash = L'/';

// ---- Error texts ----
static constexpr char s_ErrWithCode[]       = "{} (WinHTTP error {})";
static const std::string s_ErrInvalidUrl    = "Invalid HTTPS URL: ";
static const std::string s_ErrRequestFailed = "HTTP request failed";
static const std::string s_ErrQueryStatus   = "WinHttpQueryHeaders (status) failed";
static const std::string s_ErrQueryData     = "WinHttpQueryDataAvailable failed";
static const std::string s_ErrReadData      = "WinHttpReadData failed";
static const std::string s_ErrTooLarge      = "Response is too large";

void ensure(bool ok, const std::string& message) {
    if (ok) {
        return;
    }

    const DWORD code = GetLastError();

    // Certificate validation failure: retrying will not change anything.
    const bool retryable = (code != ERROR_WINHTTP_SECURE_FAILURE);
    throw NetworkError(std::format(s_ErrWithCode, message, code), retryable);
}

UrlParts parseUrl(const std::string& baseUrl) {
    const std::wstring url = utf8ToWide(baseUrl);

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
        throw std::invalid_argument(s_ErrInvalidUrl + baseUrl);
    }

    UrlParts result;
    result.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    result.port = parts.nPort;

    if (parts.dwUrlPathLength > 0) {
        result.basePath.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
    }
    while (!result.basePath.empty() && result.basePath.back() == s_PathSlash) {
        result.basePath.pop_back();
    }
    return result;
}

void sendAndReceive(HINTERNET handle, const std::wstring& headers,
                    const std::vector<unsigned char>& body) {
    const DWORD size = static_cast<DWORD>(body.size());

    LPVOID data = body.empty()
        ? WINHTTP_NO_REQUEST_DATA
        : const_cast<unsigned char*>(body.data());

    const bool sent = WinHttpSendRequest(
        handle,
        headers.c_str(),
        static_cast<DWORD>(headers.size()),
        data,
        size,
        size,
        0
    );
    const bool received = sent && WinHttpReceiveResponse(handle, nullptr);

    ensure(received, s_ErrRequestFailed);
}

int readStatus(HINTERNET handle) {
    DWORD status = 0;
    DWORD size = sizeof(status);

    ensure(WinHttpQueryHeaders(handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX),
           s_ErrQueryStatus);

    return static_cast<int>(status);
}

int readRetryAfter(HINTERNET handle) {
    wchar_t value[s_RetryAfterChars]{};
    DWORD size = sizeof(value);

    if (WinHttpQueryHeaders(handle, WINHTTP_QUERY_RETRY_AFTER, WINHTTP_HEADER_NAME_BY_INDEX,
                            value, &size, WINHTTP_NO_HEADER_INDEX)) {
        return _wtoi(value);
    }

    return 0;
}

std::vector<unsigned char> readBody(HINTERNET handle) {
    std::vector<unsigned char> body;

    for (;;) {
        DWORD available = 0;
        ensure(WinHttpQueryDataAvailable(handle, &available), s_ErrQueryData);

        if (available == 0) {
            break;
        }
        if (body.size() + available > s_MaxResponseBytes) {
            throw NetworkError(s_ErrTooLarge, false);
        }

        const std::size_t oldSize = body.size();
        body.resize(oldSize + available);

        DWORD read = 0;
        ensure(WinHttpReadData(handle, body.data() + oldSize, available, &read), s_ErrReadData);

        body.resize(oldSize + read);
        if (read == 0) {
            break;
        }
    }
    return body;
}

}