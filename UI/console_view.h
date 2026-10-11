#pragma once
#include "api.h"
#include <optional>
#include <string>
#include <vector>
#include <format>

namespace ev {

enum class MenuItem {
	Empty = -1,
    Exit = 0,
    Status = 1,
    Send = 2,
    Upload = 3,
    List = 4,
    UploadPending = 5,

};

// The only place where the program reads from and prints to the console.
class ConsoleView : public ApiListener {
public:
    // ---- Output ----
    void showMenu();
    void showStatus(const ServerStatus& status);
    void showEvent(const Event& event);
    void showEvents(const std::vector<Event>& events);
    void showUploading(long long eventId, const std::string& fileName);
    void showSkipped(long long eventId, const std::string& fileName);
    void showFailed(long long eventId, const std::string& text);
    void showPendingSummary(int uploaded, int skipped, int failed);
    void showError(const std::string& text);
    void showOffline(const std::string& reason);

    // ---- Input ----
    MenuItem ReadMenuItem();                                              // asks until the choice is valid
    std::optional<long long> ReadEventId();

    // ---- ApiListener ----
    void onWait(int seconds) override;

private:
    void print(const std::string& text);
    void printRow(  const std::string& id, const std::string& code, const std::string& file,
                    const std::string& timestamp, const std::string& image);

    template<typename... Args>
    void printFormat(const char* format, Args&&... args) {
        print(std::vformat(
            format,
            std::make_format_args(args...)
        ));
    }
};

}