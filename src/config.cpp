#include "config.h"
#include "util.h"
#include <windows.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace ev {

// ---- Configuration file ----
static const std::string s_ConfigFileName = "config.json";
static const std::string s_KeyLogDir = "log_dir";

// ---- Keys in JSON ----
static const std::string s_KeyBaseUrl  = "base_url";
static const std::string s_KeyApiKey   = "api_key";
static const std::string s_KeyLogFile  = "log_file";
static const std::string s_KeyImageDir = "image_dir";

// ---- Value validation rules ----
static const std::string s_HttpsPrefix    = "https://";
static const std::string s_KeyPlaceholder = "PUT_YOUR_KEY_HERE";

// ---- Separators in messages ----
static const std::string s_ColonSeparator = ": ";
static const std::string s_Dot            = ".";

// ---- Error messages ----
static const std::string s_ErrNotFound         = " not found (looked in the current folder and next to the exe).";
static const std::string s_ErrCannotOpen       = "cannot open file";
static const std::string s_ErrCannotReadPrefix = "Cannot read ";
static const std::string s_ErrNotSetSuffix     = " is not set in ";
static const std::string s_ErrHttpsRequired    = "base_url must start with https:// (plain HTTP is not allowed).";

static fs::path exeDir() {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return (n == 0 || n >= MAX_PATH) ? fs::path() : fs::path(buf).parent_path();
}

Config loadConfig() {
    fs::path file;

    if( fs::exists(s_ConfigFileName) ) {
        file = s_ConfigFileName;
    } else if( !exeDir().empty() && fs::exists(exeDir() / s_ConfigFileName) ) {
        file = exeDir() / s_ConfigFileName;
    }

    if( file.empty() ) {
        throw std::runtime_error(s_ConfigFileName + s_ErrNotFound);
    }

    Config c;

    try {
        std::ifstream in(file);

        if( !in ) {
            throw std::runtime_error(s_ErrCannotOpen);
		}

        const nlohmann::json j = nlohmann::json::parse(in);
        c.baseUrl  = j.value(s_KeyBaseUrl,  c.baseUrl);
        c.apiKey   = j.value(s_KeyApiKey,   c.apiKey);
        c.logDir   = j.value(s_KeyLogDir,   c.logDir);
        c.imageDir = j.value(s_KeyImageDir, c.imageDir);

    } catch (const std::exception& e) {
        throw std::runtime_error( s_ErrCannotReadPrefix + file.string() + s_ColonSeparator + e.what() );
    }

    if( c.baseUrl.empty() ) {

        throw std::runtime_error(s_KeyBaseUrl + s_ErrNotSetSuffix + s_ConfigFileName + s_Dot);
	} else if( toLower(c.baseUrl).rfind(s_HttpsPrefix, 0) != 0 ) {

        throw std::runtime_error(s_ErrHttpsRequired);
    } else if (c.apiKey.empty() || c.apiKey == s_KeyPlaceholder ) {

        throw std::runtime_error(s_KeyApiKey + s_ErrNotSetSuffix + s_ConfigFileName + s_Dot);
    }

    c.imageDir = (file.parent_path() / c.imageDir).string();
    c.logDir   = (file.parent_path() / c.logDir).string();

    return c;
}

}