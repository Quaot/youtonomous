cmake_minimum_required(VERSION 3.20)
project(Youtonomous VERSION 0.2.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra)
add_compile_definitions(UNICODE _UNICODE)

find_package(nlohmann_json REQUIRED)

add_library(core STATIC
    src/timecode.cpp
    src/library.cpp
    src/settings.cpp
    src/session.cpp
    src/study_log.cpp
)
target_include_directories(core PUBLIC src)
target_link_libraries(core PUBLIC nlohmann_json::nlohmann_json)

add_library(players STATIC
    src/player/vlc_player.cpp
    src/player/mpv_player.cpp
    src/player/make_player.cpp
)
target_include_directories(players PUBLIC src/player)
target_link_libraries(players PUBLIC vlc)

configure_file(packaging/app.rc.in app.rc @ONLY)

add_executable(youtonomous WIN32
    src/main.cpp
    src/downloader.cpp
    src/ui/controls.cpp
    src/ui/library_panel.cpp
    src/ui/player_panel.cpp
    src/ui/marks_panel.cpp
    src/ui/main_window.cpp
    src/ui/seekbar.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/app.rc
)
target_include_directories(youtonomous PRIVATE src/ui)
target_link_libraries(youtonomous PRIVATE core players comdlg32 comctl32)
target_link_options(youtonomous PRIVATE -municode)

enable_testing()

add_executable(timecode_test tests/timecode_test.cpp)
target_link_libraries(timecode_test PRIVATE core)
add_test(NAME timecode COMMAND timecode_test)

add_executable(library_test tests/library_test.cpp)
target_link_libraries(library_test PRIVATE core)
add_test(NAME library COMMAND library_test)

add_executable(settings_test tests/settings_test.cpp)
target_link_libraries(settings_test PRIVATE core)
add_test(NAME settings COMMAND settings_test)

add_executable(session_test tests/session_test.cpp)
target_link_libraries(session_test PRIVATE core)
add_test(NAME session COMMAND session_test)

add_executable(study_log_test tests/study_log_test.cpp)
target_link_libraries(study_log_test PRIVATE core)
add_test(NAME study_log COMMAND study_log_test)

add_executable(text_test tests/text_test.cpp)
target_include_directories(text_test PRIVATE src)
add_test(NAME text COMMAND text_test)

add_executable(seekbar_test tests/seekbar_test.cpp src/ui/seekbar.cpp)
target_include_directories(seekbar_test PRIVATE src/ui)
target_link_libraries(seekbar_test PRIVATE core comctl32)
add_test(NAME seekbar COMMAND seekbar_test)

add_executable(player_test tests/player_test.cpp)
target_link_libraries(player_test PRIVATE core players)
add_test(NAME player_vlc COMMAND player_test vlc)
set_tests_properties(player_vlc PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
add_test(NAME player_mpv COMMAND player_test mpv)
set_tests_properties(player_mpv PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)

add_executable(downloader_test tests/downloader_test.cpp src/downloader.cpp)
target_link_libraries(downloader_test PRIVATE core)
add_test(NAME downloader COMMAND downloader_test)
set_tests_properties(downloader PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)

add_executable(ui_smoke_test tests/ui_smoke_test.cpp)
target_link_libraries(ui_smoke_test PRIVATE core)
add_test(NAME ui_smoke COMMAND ui_smoke_test $<TARGET_FILE:youtonomous>)
set_tests_properties(ui_smoke PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300)
