# Event API client (Windows, C++)

Console client for the HTTPS Event API (`https://challenge.spialert.com/api/v1`),
built on WinHTTP and nlohmann/json.

## Build

**Requirements**

- Windows 10 or 11
- Visual Studio 2026 (18.9.1) with the workload *Desktop development with C++*
  (MSVC, Windows SDK, C++ CMake tools for Windows)
- CMake 3.20 or newer (developed with 4.2.1)
- C++20 (the project uses `std::format`)

**Dependencies:** nothing to install. nlohmann/json 3.12.0 (MIT) is in `third_party/nlohmann/json.hpp`.
WinHTTP and Ole32 come with the Windows SDK.

**Steps**

1. Clone the repository.
2. Create `config.json` first (see below), so CMake copies it next to the executable.
3. In Visual Studio: **File → Open → Folder**, select the repository root,
   wait for `CMake generation finished`.
4. Select **x64-Debug** and run **Build → Build All**.

Or from *Developer PowerShell for VS*:

```
cmake --preset x64-debug
cmake --build out/build/x64-debug
```

The result is in `out\build\x64-debug`: `eventclient.exe`, `tests.exe` (unit tests),
and copies of `config.json` and `Resources\`.

## Configure and run

Copy `config.example.json` to `config.json` and fill in your actual base URL and API key.

```json
{
  "base_url": "URL",
  "api_key": "PERSONAL_KEY",
  "log_dir": "Logs",
  "image_dir": "Resources"
}
```

- `log_dir` and `image_dir` are optional (defaults as above). Relative paths are resolved from the folder of `config.json`.
- `config.json` is searched for in the current folder, then next to the exe. It is in `.gitignore`: never commit your key.

Start `eventclient.exe` (in Visual Studio choose it in *Select Startup Item* and press Ctrl+F5):

```
1. Check server status
2. Send event (random code 1-10, random image from image_dir, current UTC time)
3. Upload image for an existing event (asks for the event id)
4. List events (events without an image are marked NO IMAGE)
5. Upload pending images (all events without an image)
0. Exit
```

Typical session: `1` → `2` → `4` → `3` with the event id → `4`.
(`A-Z a-z 0-9 _ -`, lowercase extension). Logs go to `Logs/eventclient_<UTC date>.log`.

## Known limitations

- No local validation of file name or content: the server checks them and its error message is shown.
- The image upload is not retried automatically after a network error (the server may already have the file).
  Menu items 3 and 5 read the real state from the server, so repeating is safe.
- the API has no "get event by id".
- Menu item 2 picks the image at random, it cannot be chosen.
- `Retry-After` is read only as a number of seconds, an HTTP date becomes a 1 second wait.
- Non-ASCII paths in `image_dir` and `log_dir` may not work.
- Unit tests use a fake HTTP client, so TLS, headers and multipart are checked only against the real server.

## What I would improve

- An integration test against a local mock HTTPS server.
- Local validation of the file and an SHA-256 check after the upload.
- Choosing the image in menu item 2, support for `Retry-After` dates, retry settings in the config.
- Log rotation.
