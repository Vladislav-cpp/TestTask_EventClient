#pragma once

#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>

namespace ev::winhttp {

struct UrlParts {
    std::wstring host;
    std::wstring basePath;
    INTERNET_PORT port = 0;
};

void ensure(bool ok, const std::string& message);

UrlParts parseUrl(const std::string& baseUrl);

void sendAndReceive(HINTERNET handle, const std::wstring& headers,
                    const std::vector<unsigned char>& body);

int readStatus(HINTERNET handle);

int readRetryAfter(HINTERNET handle);

std::vector<unsigned char> readBody(HINTERNET handle);

}