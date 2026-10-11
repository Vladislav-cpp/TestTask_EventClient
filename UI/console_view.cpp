#include "console_view.h"
#include "util.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <utility>

namespace ev {

// ---- Menu ----
static const std::string s_MenuText =
    "\n=== Event API client ===\n"
    "1. Check server status\n"
    "2. Send event (random code 1-10, current UTC time)\n"
    "3. Upload image for an existing event (asks for event id)\n"
    "4. List events\n"
    "5. Upload pending images\n"
    "0. Exit\n";

// ---- Requests ----
static const char s_NewLine = '\n';

// ---- Texts ----
static constexpr char s_Status[]  = "Server is {}\n  API version : {}\n  Server time : {}";
static constexpr char s_EventFields[] =
    "Event:\n"
    "  event_id       : {}\n"
    "  event_uid      : {}\n"
    "  event_code     : {}\n"
    "  file_name      : {}\n"
    "  timestamp      : {}\n"
    "  image_uploaded : {}";

static const std::string s_Yes            = "yes";
static const std::string s_No             = "no";
static constexpr char s_Uploading[]      = "[upload] event {} <- {}";
static constexpr char s_Skipped[]        = "[skip] event {}: local file not found: {}";
static constexpr char s_Failed[]         = "[fail] event {}: {}";
static constexpr char s_PendingSummary[] = "Done: {} uploaded, {} skipped, {} failed.";
static constexpr char s_Error[]          = "Error: {}";
static constexpr char s_Waiting[]        = "Waiting {} s before retrying...";

static constexpr char s_PromptEventId[] = "Event id (Enter = cancel): ";
static constexpr char s_BadEventId[]    = "Event id must be a positive number.";

// ---- Table ----
static const std::string s_ColId         = "ID";
static const std::string s_ColCode       = "CODE";
static const std::string s_ColFile       = "FILE";
static const std::string s_ColTimestamp  = "TIMESTAMP";
static const std::string s_ColImage      = "IMAGE";
static const std::string s_ImageUploaded = "uploaded";
static const std::string s_ImageMissing  = "NO IMAGE";
static constexpr char s_TableSummary[]  = "{} event(s), {} without image.";

static const std::string s_PromptChoice  = "Choose: ";
static const std::string s_UnknownChoice = "Unknown choice.";
static constexpr char s_Offline[] = "Server is OFFLINE or unreachable: {}";

static const int s_WidthId        = 8;
static const int s_WidthCode      = 6;
static const int s_WidthFile      = 38;
static const int s_WidthTimestamp = 26;

// ===================== Output =====================

void ConsoleView::print(const std::string& text) {
    std::cout << text << s_NewLine;
}

void ConsoleView::printRow(const std::string& id, const std::string& code, const std::string& file,
                     const std::string& timestamp, const std::string& image) {
    std::cout << std::left << std::setw(s_WidthId) << id << std::setw(s_WidthCode) << code
              << std::setw(s_WidthFile) << file << std::setw(s_WidthTimestamp) << timestamp
              << image << s_NewLine;
}

void ConsoleView::showMenu() {
    std::cout << s_MenuText;
}

void ConsoleView::showStatus(const ServerStatus& status) {
    printFormat( s_Status, status.status, status.api_version, status.server_time );
}

void ConsoleView::showEvent(const Event& event) {
    printFormat(s_EventFields, event.event_id, event.event_uid, event.event_code,
                event.file_name, event.timestamp, event.image_uploaded ? s_Yes : s_No);
}

void ConsoleView::showEvents(const std::vector<Event>& events) {
    printRow(s_ColId, s_ColCode, s_ColFile, s_ColTimestamp, s_ColImage);

    for(const Event& event : events) {

        printRow(std::to_string(event.event_id), std::to_string(event.event_code), event.file_name,
                 event.timestamp, event.image_uploaded ? s_ImageUploaded : s_ImageMissing);
    }

    const auto missing = std::count_if(events.begin(), events.end(),
                                       [](const Event& event) { return !event.image_uploaded; });
    printFormat(s_TableSummary, events.size(), missing);
}

void ConsoleView::showUploading(long long eventId, const std::string& fileName) {
    printFormat(s_Uploading, eventId, fileName);
}

void ConsoleView::showSkipped(long long eventId, const std::string& fileName) {
    printFormat(s_Skipped, eventId, fileName);
}

void ConsoleView::showFailed(long long eventId, const std::string& text) {
    printFormat(s_Failed, eventId, text);
}

void ConsoleView::showPendingSummary(int uploaded, int skipped, int failed) {
    printFormat(s_PendingSummary, uploaded, skipped, failed);
}

void ConsoleView::showError(const std::string& text) {
    printFormat(s_Error, text);
}

void ConsoleView::showOffline(const std::string& reason) {
    printFormat(s_Offline, reason);
}

void ConsoleView::onWait(int seconds) {
    printFormat(s_Waiting, seconds);
}

// ===================== Input =====================

MenuItem ConsoleView::ReadMenuItem() {
    std::cout << s_PromptChoice << std::flush;

    std::string input;
    if (!std::getline(std::cin, input)) {
        return MenuItem::Exit;   // end of input (Ctrl+Z)
    }

    int value = 0;
    const auto [ptr, error] = std::from_chars(
        input.data(),
        input.data() + input.size(),
        value
    );

    if (error != std::errc{}
        || ptr != input.data() + input.size()
        || value < static_cast<int>(MenuItem::Exit)
        || value > static_cast<int>(MenuItem::UploadPending)) {

        print(s_UnknownChoice);
        return MenuItem::Empty;
    }

    return static_cast<MenuItem>(value);
}

std::optional<long long> ConsoleView::ReadEventId() {
    std::cout << s_PromptEventId << std::flush;

    std::string input;
    if (!std::getline(std::cin, input) || input.empty()) {
        return std::nullopt;
    }

    long long value = 0;
    const auto [ptr, error] = std::from_chars(
        input.data(),
        input.data() + input.size(),
        value
    );

    if (error != std::errc{}
        || ptr != input.data() + input.size()
        || value < 1) {

        print(s_BadEventId);
        return std::nullopt;
    }

    return value;
}

}