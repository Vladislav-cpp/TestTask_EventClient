cmake_minimum_required(VERSION 3.20)
project(EventClient LANGUAGES CXX)

# C++20 for std::format
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(eventclient
    src/main.cpp
    src/application.cpp
    src/config.cpp
    src/api.cpp
    utility/util.cpp
    UI/console_view.cpp
    Http/HttpClient.cpp
    Http/winhttp_helpers.cpp
    utility/Logger.cpp
)

target_include_directories(eventclient PRIVATE
    src
    Http
    UI
    utility
    third_party
)

# winhttp: HTTPS, bcrypt: SHA-256, ole32: CoCreateGuid
target_link_libraries(eventclient PRIVATE winhttp ole32)

target_compile_definitions(eventclient PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)

if(MSVC)
    target_compile_options(eventclient PRIVATE /utf-8 /W4 /permissive-)
endif()

# Copy config.json next to the exe (the file is in .gitignore, the key is not tracked by git)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.json")
    add_custom_command(TARGET eventclient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
                "$<TARGET_FILE_DIR:eventclient>/config.json")
endif()

set(RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources")

if(EXISTS "${RESOURCES_DIR}")
    add_custom_target(copy_resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${RESOURCES_DIR}"
                "${CMAKE_CURRENT_BINARY_DIR}/Resources"
        COMMENT "Copying Resources next to eventclient.exe")

    add_dependencies(eventclient copy_resources)
else()
    message(WARNING "Resources folder not found: ${RESOURCES_DIR}")
endif()

option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    add_executable(tests tests/tests.cpp src/api.cpp utility/util.cpp utility/Logger.cpp)
    target_include_directories(tests PRIVATE src Http UI utility third_party)
    target_link_libraries(tests PRIVATE winhttp ole32)
    target_compile_definitions(tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(tests PRIVATE /utf-8 /W4 /permissive-)
    endif()
endif()