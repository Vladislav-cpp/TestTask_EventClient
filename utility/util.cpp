#include "util.h"
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <format>
#include <random>

namespace fs = std::filesystem;

static const std::vector<std::string> s_ImageExtensions = { ".png", ".jpg", ".jpeg" };
static constexpr char s_ErrNoImages[] = "No .png/.jpg/.jpeg images found in: {}";

static std::mt19937 s_Generator{ std::random_device{}() };

namespace ev {

std::string fileNameOf(const std::string& path) {
    return fs::path(path).filename().string();
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) throw std::runtime_error("Invalid UTF-8 string");
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string generateEventUid() {
    GUID g{};
    if (FAILED(CoCreateGuid(&g))) throw std::runtime_error("CoCreateGuid failed");
    char buf[40];
    std::snprintf(buf, sizeof buf, "%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  (unsigned long)g.Data1, (unsigned)g.Data2, (unsigned)g.Data3,
                  (unsigned)g.Data4[0], (unsigned)g.Data4[1], (unsigned)g.Data4[2], (unsigned)g.Data4[3],
                  (unsigned)g.Data4[4], (unsigned)g.Data4[5], (unsigned)g.Data4[6], (unsigned)g.Data4[7]);
    return buf;
}

std::string utcNowIso() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
    if (gmtime_s(&tm, &t) != 0) throw std::runtime_error("gmtime_s failed");
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::vector<unsigned char> readFile(const fs::path& path, std::size_t maxBytes) {
    const std::string shown = path.string();   // лише для тексту помилок

    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        throw std::runtime_error("File not found: " + shown);
    }

    const auto size = fs::file_size(path, ec);
    if (ec) {
        throw std::runtime_error("Cannot read file size: " + shown);
    }
    if (size == 0) {
        throw std::runtime_error("File is empty: " + shown);
    }
    if (size > maxBytes) {
        throw std::runtime_error("File is larger than the limit (" + std::to_string(maxBytes) + " bytes): " + shown);
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open file: " + shown);
    }

    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (static_cast<std::size_t>(in.gcount()) != data.size()) {
        throw std::runtime_error("Error while reading file: " + shown);
    }

    return data;
}
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string pickRandomImage(const std::string& dir, std::size_t maxBytes) {
    std::vector<std::string> names;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        std::error_code sizeEc;
        const auto size = entry.is_regular_file() ? entry.file_size(sizeEc) : 0;
        const std::string extension = entry.path().extension().string();

        if (!sizeEc && size > 0 && size <= maxBytes &&
            std::find(s_ImageExtensions.begin(), s_ImageExtensions.end(), extension) != s_ImageExtensions.end()) {

            names.push_back(entry.path().filename().string());
        }
    }

    if (names.empty()) {
        throw std::runtime_error(std::format(s_ErrNoImages, dir));
    }

    return names[std::uniform_int_distribution<std::size_t>(0, names.size() - 1)(s_Generator)];
}

}