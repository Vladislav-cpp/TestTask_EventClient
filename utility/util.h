#pragma once
#include <ctime>
#include <string>
#include <vector>
#include <filesystem>

// Дрібні допоміжні функції без знання про Event API.
namespace ev {

std::string fileNameOf(const std::string& path);

std::wstring utf8ToWide(const std::string& s);

std::string generateEventUid();   // GUID без дужок, 36 символів
std::string utcNowIso();          // "2026-10-05T12:34:56Z"

// Читає файл у бінарному режимі. Кидає std::runtime_error (немає / порожній / завеликий).
std::vector<unsigned char> readFile(const std::filesystem::path& path, std::size_t maxBytes);


std::string toLower(std::string s);

std::string pickRandomImage(const std::string& dir, std::size_t maxBytes);
}