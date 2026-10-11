#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include <windows.h>

#include <commctrl.h>

#include <nlohmann/json.hpp>

#include "check.h"
#include "timecode.h"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

struct Search {
    DWORD pid = 0;
    const wchar_t* class_name = nullptr;
    std::vector<HWND> found;
};

static bool waitFor(const std::function<bool()>& condition, int milliseconds)
{
    Clock::time_point end = Clock::now() + std::chrono::milliseconds(milliseconds);
    while (!condition()) {
        if (Clock::now() >= end)
            return false;
        Sleep(100);
    }
    return true;
}

static std::wstring environmentWith(const fs::path& home)
{
    std::wstring block;
    wchar_t* strings = GetEnvironmentStringsW();
    for (wchar_t* entry = strings; *entry; entry += wcslen(entry) + 1) {
        bool replaced = _wcsnicmp(entry, L"APPDATA=", 8) == 0 || _wcsnicmp(entry, L"USERPROFILE=", 12) == 0;
        if (!replaced)
            block += std::wstring(entry) + L'\0';
    }
    FreeEnvironmentStringsW(strings);
    block += L"APPDATA=" + (home / "AppData").wstring() + L'\0';
    block += L"USERPROFILE=" + home.wstring() + L'\0';
    return block + L'\0';
}

static bool launch(std::wstring command_line, std::wstring* environment, PROCESS_INFORMATION& process)
{
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    DWORD flags = CREATE_UNICODE_ENVIRONMENT;
    if (!environment)
        flags |= CREATE_NO_WINDOW;
    LPVOID block = environment ? environment->data() : nullptr;
    BOOL started = CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, FALSE, flags, block, nullptr, &startup, &process);
    return started != 0;
}

static bool runToEnd(const std::wstring& command_line)
{
    PROCESS_INFORMATION process{};
    if (!launch(command_line, nullptr, process))
        return false;
    bool finished = WaitForSingleObject(process.hProcess, 120000) == WAIT_OBJECT_0;
    DWORD code = 1;
    if (finished)
        GetExitCodeProcess(process.hProcess, &code);
    else
        TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return finished && code == 0;
}

static bool makeClip(const fs::path& file)
{
    wchar_t ffmpeg[MAX_PATH];
    if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, ffmpeg, nullptr) == 0)
        return false;

    std::wstring command = L"\"" + std::wstring(ffmpeg) + L"\" -hide_banner -loglevel error -y";
    command += L" -f lavfi -i testsrc=duration=40:size=320x240:rate=25";
    command += L" -f lavfi -i sine=frequency=440:duration=40";
    command += L" -af volume=0 -c:v mpeg4 -c:a aac -shortest";
    command += L" \"" + file.wstring() + L"\"";
    return runToEnd(command) && fs::exists(file);
}

static BOOL CALLBACK findApp(HWND hwnd, LPARAM param)
{
    auto* search = reinterpret_cast<Search*>(param);
    wchar_t name[64];
    DWORD owner = 0;
    GetWindowThreadProcessId(hwnd, &owner);
    if (owner == search->pid && GetClassNameW(hwnd, name, 64) && wcscmp(name, L"Youtonomous") == 0) {
        search->found.push_back(hwnd);
        return FALSE;
    }
    return TRUE;
}

static BOOL CALLBACK findChild(HWND hwnd, LPARAM param)
{
    auto* search = reinterpret_cast<Search*>(param);
    wchar_t name[64];
    if (GetClassNameW(hwnd, name, 64) && _wcsicmp(name, search->class_name) == 0)
        search->found.push_back(hwnd);
    return TRUE;
}

static HWND appWindow(DWORD pid)
{
    Search search;
    search.pid = pid;
    EnumWindows(findApp, reinterpret_cast<LPARAM>(&search));
    return search.found.empty() ? nullptr : search.found[0];
}

static std::vector<HWND> children(HWND top, const wchar_t* class_name)
{
    Search search;
    search.class_name = class_name;
    EnumChildWindows(top, findChild, reinterpret_cast<LPARAM>(&search));
    return search.found;
}

static std::wstring textOf(HWND hwnd)
{
    wchar_t buffer[512] = L"";
    SendMessageTimeoutW(hwnd, WM_GETTEXT, 512, reinterpret_cast<LPARAM>(buffer), SMTO_ABORTIFHUNG, 2000, nullptr);
    return buffer;
}

static HWND button(HWND top, const std::wstring& caption)
{
    for (HWND hwnd : children(top, L"Button")) {
        if (textOf(hwnd) == caption)
            return hwnd;
    }
    return nullptr;
}

static HWND buttonStartingWith(HWND top, const std::wstring& prefix)
{
    for (HWND hwnd : children(top, L"Button")) {
        if (textOf(hwnd).rfind(prefix, 0) == 0)
            return hwnd;
    }
    return nullptr;
}

static bool click(HWND top, const std::wstring& caption)
{
    HWND hwnd = button(top, caption);
    if (!hwnd)
        return false;
    SendMessageTimeoutW(hwnd, BM_CLICK, 0, 0, SMTO_ABORTIFHUNG, 2000, nullptr);
    return true;
}

static void setText(HWND hwnd, const std::wstring& text)
{
    SendMessageTimeoutW(hwnd, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()), SMTO_ABORTIFHUNG, 2000, nullptr);
}

static void key(HWND top, WPARAM vk)
{
    PostMessageW(top, WM_KEYDOWN, vk, 0);
}

static int playbackTime(HWND top)
{
    for (HWND hwnd : children(top, L"Static")) {
        std::wstring text = textOf(hwnd);
        size_t slash = text.find(L" / ");
        if (slash == std::wstring::npos)
            continue;
        std::optional<int> seconds = timecode::parse(std::string(text.begin(), text.begin() + slash));
        return seconds ? *seconds : -1;
    }
    return -1;
}

static std::wstring labelStartingWith(HWND top, const std::wstring& prefix)
{
    for (HWND hwnd : children(top, L"Static")) {
        std::wstring text = textOf(hwnd);
        if (text.rfind(prefix, 0) == 0)
            return text;
    }
    return L"";
}

static int markCount(HWND top)
{
    std::vector<HWND> lists = children(top, L"ListBox");
    if (lists.size() < 2)
        return -1;
    return static_cast<int>(SendMessageW(lists[1], LB_GETCOUNT, 0, 0));
}

static std::wstring speedShown(HWND top)
{
    std::vector<HWND> boxes = children(top, L"ComboBox");
    return boxes.empty() ? std::wstring() : textOf(boxes[0]);
}

static int volumeShown(HWND top)
{
    std::vector<HWND> bars = children(top, L"msctls_trackbar32");
    if (bars.empty())
        return -1;
    return static_cast<int>(SendMessageW(bars[0], TBM_GETPOS, 0, 0));
}

static nlohmann::json savedVideo(const fs::path& library_file)
{
    std::ifstream in(library_file);
    nlohmann::json items = nlohmann::json::parse(in, nullptr, false);
    if (items.is_array() && !items.empty())
        return items[0];
    return nlohmann::json::object();
}

static nlohmann::json readJson(const fs::path& file)
{
    std::ifstream in(file);
    nlohmann::json item = nlohmann::json::parse(in, nullptr, false);
    return item.is_object() ? item : nlohmann::json::object();
}

static bool hasMark(const nlohmann::json& video, const std::string& label)
{
    for (const nlohmann::json& mark : video.value("marks", nlohmann::json::array())) {
        if (mark.value("label", "") == label)
            return true;
    }
    return false;
}

static bool closeApp(PROCESS_INFORMATION& process, HWND top)
{
    PostMessageW(top, WM_CLOSE, 0, 0);
    bool closed = WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0;
    if (!closed)
        TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return closed;
}

static bool within(int value, int low, int high)
{
    return value >= low && value <= high;
}

static std::string got(int value)
{
    return ", got " + std::to_string(value);
}

static std::string narrowText(const std::wstring& text)
{
    std::string result;
    for (wchar_t c : text)
        result += c < 128 ? static_cast<char>(c) : '?';
    return result;
}

static int markIndex(HWND top, const std::wstring& text)
{
    std::vector<HWND> lists = children(top, L"ListBox");
    if (lists.size() < 2)
        return -1;
    int count = static_cast<int>(SendMessageW(lists[1], LB_GETCOUNT, 0, 0));
    for (int i = 0; i < count; ++i) {
        wchar_t line[512] = L"";
        if (SendMessageW(lists[1], LB_GETTEXTLEN, i, 0) >= 512)
            continue;
        SendMessageW(lists[1], LB_GETTEXT, i, reinterpret_cast<LPARAM>(line));
        if (std::wstring(line).find(text) != std::wstring::npos)
            return i;
    }
    return -1;
}

static void selectMark(HWND top, int index)
{
    std::vector<HWND> lists = children(top, L"ListBox");
    if (lists.size() < 2)
        return;
    SendMessageW(lists[1], LB_SETCURSEL, index, 0);
    WPARAM command = MAKEWPARAM(GetDlgCtrlID(lists[1]), LBN_SELCHANGE);
    SendMessageW(top, WM_COMMAND, command, reinterpret_cast<LPARAM>(lists[1]));
}

static bool hasNote(const nlohmann::json& video, const std::string& label, const std::string& note)
{
    for (const nlohmann::json& mark : video.value("marks", nlohmann::json::array())) {
        if (mark.value("label", "") == label && mark.value("note", "") == note)
            return true;
    }
    return false;
}

static void clickSeekBar(HWND top, int numerator, int denominator)
{
    std::vector<HWND> bars = children(top, L"YoutonomousSeekBar");
    if (bars.empty())
        return;
    RECT rect;
    GetClientRect(bars[0], &rect);
    LPARAM point = MAKELPARAM(rect.right * numerator / denominator, rect.bottom / 2);
    SendMessageW(bars[0], WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(bars[0], WM_LBUTTONUP, 0, point);
}

static void firstRun(const std::wstring& command_line, std::wstring& environment, const fs::path& library_file)
{
    PROCESS_INFORMATION process{};
    check(launch(command_line, &environment, process), "app starts");
    HWND top = nullptr;
    check(waitFor([&] { return (top = appWindow(process.dwProcessId)) != nullptr; }, 10000), "main window appears");
    if (!top)
        return;

    check(waitFor([&] { return playbackTime(top) >= 1; }, 15000), "the clip plays");
    check(waitFor([&] { return click(top, L"Pause"); }, 5000), "Pause button is shown while playing");
    check(waitFor([&] { return button(top, L"Play") != nullptr; }, 5000), "Play button is shown after pausing");

    int paused = playbackTime(top);
    click(top, L"+10");
    check(waitFor([&] { return within(playbackTime(top), paused + 9, paused + 11); }, 5000), "+10 moves forward 10 s" + got(playbackTime(top)));
    click(top, L"-10");
    check(waitFor([&] { return within(playbackTime(top), paused - 1, paused + 1); }, 5000), "-10 moves back 10 s" + got(playbackTime(top)));

    check(children(top, L"YoutonomousSeekBar").size() == 1, "one seek bar is shown");
    clickSeekBar(top, 3, 4);
    check(waitFor([&] { return within(playbackTime(top), 28, 32); }, 5000), "clicking three quarters along the bar seeks to about 30 s" + got(playbackTime(top)));

    int before = markCount(top);
    key(top, 'B');
    check(waitFor([&] { return markCount(top) == before + 1; }, 3000), "B adds a bookmark" + got(markCount(top)));

    std::vector<HWND> edits = children(top, L"Edit");
    check(edits.size() >= 3, "URL, start and bookmark label boxes exist");
    if (edits.size() >= 3) {
        setText(edits[2], L"Smoke mark");
        click(top, L"Add");
        check(waitFor([&] { return hasMark(savedVideo(library_file), "Smoke mark"); }, 3000), "Add saves a labelled bookmark");

        check(edits.size() >= 4, "a note box exists");
        int smoke_index = markIndex(top, L"Smoke mark");
        check(smoke_index >= 0, "the labelled bookmark is listed");
        if (smoke_index >= 0 && edits.size() >= 4) {
            selectMark(top, smoke_index);
            click(top, L"Edit");
            check(waitFor([&] { return button(top, L"Save") != nullptr; }, 3000), "Edit turns Add into Save");
            setText(edits[2], L"Renamed mark");
            setText(edits[3], L"line one\r\nline two");
            click(top, L"Save");
            check(waitFor([&] { return hasNote(savedVideo(library_file), "Renamed mark", "line one\nline two"); }, 3000),
                  "Save renames the bookmark and stores its note");
            check(waitFor([&] { return button(top, L"Add") != nullptr; }, 3000), "after saving the button reads Add again");
            check(markIndex(top, L"Renamed mark") >= 0, "the list shows the new name");
        }

        setText(edits[1], L"0:20");
        click(top, L"Save start");
        check(waitFor([&] { return savedVideo(library_file).value("start", -1) == 20; }, 3000), "Save start saves the start point");
    }

    HWND carry = button(top, L"Carry on");
    check(carry != nullptr && !IsWindowEnabled(carry), "carrying on is off until you have stopped somewhere");
    check(button(top, L"Break") != nullptr, "a break button is shown");
    check(button(top, L"Save start") != nullptr, "the start point button has a name of its own");

    key(top, VK_HOME);
    check(waitFor([&] { return within(playbackTime(top), 20, 21); }, 5000), "Home goes to the start point" + got(playbackTime(top)));
    key(top, VK_RIGHT);
    check(waitFor([&] { return within(playbackTime(top), 30, 31); }, 5000), "Right arrow moves forward 10 s" + got(playbackTime(top)));

    check(speedShown(top) == L"1x", "speed starts at 1x");
    key(top, VK_OEM_PLUS);
    key(top, VK_OEM_PLUS);
    check(waitFor([&] { return speedShown(top) == L"1.5x"; }, 3000), "= pressed twice shows 1.5x");

    std::vector<HWND> volume_bars = children(top, L"msctls_trackbar32");
    check(volume_bars.size() == 1, "one volume slider is shown");
    if (!volume_bars.empty()) {
        SendMessageW(volume_bars[0], TBM_SETPOS, TRUE, 40);
        SendMessageW(top, WM_HSCROLL, MAKEWPARAM(TB_ENDTRACK, 0), reinterpret_cast<LPARAM>(volume_bars[0]));
    }

    check(closeApp(process, top), "app closes cleanly");
}

static void secondRun(const std::wstring& command_line, std::wstring& environment, const fs::path& library_file)
{
    int stopped = savedVideo(library_file).value("position", 0);
    check(within(stopped, 30, 39), "closing remembers where playback stopped" + got(stopped));

    PROCESS_INFORMATION process{};
    check(launch(command_line, &environment, process), "app starts again");
    HWND top = nullptr;
    check(waitFor([&] { return (top = appWindow(process.dwProcessId)) != nullptr; }, 10000), "main window appears again");
    if (!top)
        return;

    RECT rect{};
    GetWindowRect(top, &rect);
    std::string position = std::to_string(rect.left) + "," + std::to_string(rect.top);
    std::string size = std::to_string(rect.right - rect.left) + "x" + std::to_string(rect.bottom - rect.top);
    check(within(rect.left, 48, 52) && within(rect.top, 58, 62), "window opens at the saved position, got " + position);
    check(within(rect.right - rect.left, 998, 1002) && within(rect.bottom - rect.top, 698, 702), "window opens at the saved size, got " + size);

    int first = -1;
    waitFor([&] { return (first = playbackTime(top)) >= 1; }, 15000);
    check(within(first, 19, 25), "reopened video starts at the saved start point" + got(first));
    check(waitFor([&] { return markCount(top) >= 2; }, 3000), "bookmarks survive a restart" + got(markCount(top)));
    check(speedShown(top) == L"1.5x", "the saved speed is shown after a restart");

    std::vector<HWND> boxes = children(top, L"ComboBox");
    check(boxes.size() >= 2, "a pitch box is shown next to the speed box");
    if (boxes.size() >= 2)
        check(textOf(boxes[1]) == L"+2", "the saved pitch is shown after a restart, got " + narrowText(textOf(boxes[1])));
    HWND keep_pitch = button(top, L"Keep pitch");
    check(keep_pitch != nullptr, "a keep pitch box is shown");
    if (keep_pitch)
        check(SendMessageW(keep_pitch, BM_GETCHECK, 0, 0) == BST_UNCHECKED, "the saved keep pitch setting is shown after a restart");
    check(volumeShown(top) == 40, "the saved volume is shown after a restart" + got(volumeShown(top)));

    check(waitFor([&] { return labelStartingWith(top, L"Pomodoro 1") != L""; }, 5000),
          "the session row names the block the video opens in");
    fs::path study_file = library_file.parent_path() / "study.json";
    auto studied = [&] {
        nlohmann::json entries = nlohmann::json::parse(std::ifstream(study_file), nullptr, false);
        if (!entries.is_array())
            return 0;
        int seconds = 0;
        for (const nlohmann::json& entry : entries) {
            if (entry.is_object() && entry.value("label", "") == "Pomodoro 1")
                seconds += entry.value("seconds", 0);
        }
        return seconds;
    };
    check(waitFor([&] { return studied() >= 4; }, 20000), "a finished work block is logged" + got(studied()));
    check(studied() <= 10, "the logged block counts real time, not sped-up time" + got(studied()));
    check(labelStartingWith(top, L"Studied today") != L"", "the totals line is shown");

    HWND silence = button(top, L"Silent study");
    check(silence != nullptr, "a silent study box is shown");
    if (silence) {
        SendMessageW(silence, BM_CLICK, 0, 0);
        check(waitFor([&] { return SendMessageW(silence, BM_GETCHECK, 0, 0) == BST_CHECKED; }, 2000),
              "the silent study box ticks");
    }

    check(waitFor([&] { return labelStartingWith(top, L"Break 1") != L""; }, 10000),
          "the session moves on to the break once the block is done");
    check(button(top, L"Study") != nullptr, "the break button offers a way back to studying");
    key(top, 'K');
    HWND prompt = nullptr;
    waitFor([&] { return (prompt = FindWindowExW(nullptr, nullptr, L"#32770", L"Youtonomous")) != nullptr; }, 800);
    if (prompt)
        PostMessageW(prompt, WM_COMMAND, IDCANCEL, 0);
    check(prompt == nullptr, "leaving a break asks nothing");
    check(waitFor([&] { return playbackTime(top) >= 34; }, 3000),
          "leaving a break early goes back to work" + got(playbackTime(top)));
    check(waitFor([&] { return labelStartingWith(top, L"Pomodoro 2") != L""; }, 2000),
          "the session names the work block it returns to");

    HWND carry_on = buttonStartingWith(top, L"Carry on from");
    check(carry_on != nullptr && IsWindowEnabled(carry_on), "the carry on button names where playback stopped");
    key(top, 'R');
    check(waitFor([&] { return within(playbackTime(top), stopped - 1, stopped + 4); }, 5000), "R resumes where playback stopped" + got(playbackTime(top)));

    MONITORINFO monitor{};
    monitor.cbSize = sizeof monitor;
    GetMonitorInfoW(MonitorFromWindow(top, MONITOR_DEFAULTTONEAREST), &monitor);
    auto fills_screen = [&] {
        RECT now{};
        GetWindowRect(top, &now);
        return EqualRect(&now, &monitor.rcMonitor) != 0;
    };
    auto restored = [&] {
        RECT now{};
        GetWindowRect(top, &now);
        return within(now.left, 48, 52) && within(now.top, 58, 62) && within(now.right - now.left, 998, 1002);
    };
    std::vector<HWND> lists = children(top, L"ListBox");

    key(top, 'F');
    check(waitFor(fills_screen, 3000), "F fills the screen");
    check(!lists.empty() && !IsWindowVisible(lists[0]), "fullscreen hides the library");
    key(top, VK_ESCAPE);
    check(waitFor(restored, 3000), "Esc restores the window's size and position");
    check(!lists.empty() && IsWindowVisible(lists[0]), "leaving fullscreen shows the library again");
    key(top, 'F');
    check(waitFor(fills_screen, 3000), "F fills the screen again");

    check(closeApp(process, top), "app closes cleanly again");
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: ui_smoke_test <path to youtonomous.exe>\n");
        return 2;
    }

    fs::path root = fs::temp_directory_path() / ("youtonomous-ui-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root / "AppData");

    fs::path clip = root / "clip.mp4";
    if (!makeClip(clip)) {
        std::printf("SKIP: ffmpeg not found or could not make a test clip\n");
        fs::remove_all(root, error);
        return 77;
    }

    std::wstring command_line = L"\"" + fs::u8path(argv[1]).wstring() + L"\" \"" + clip.wstring() + L"\"";
    std::wstring environment = environmentWith(root);
    fs::path library_file = root / "AppData" / "Youtonomous" / "library.json";
    fs::path settings_file = root / "AppData" / "Youtonomous" / "settings.json";

    firstRun(command_line, environment, library_file);

    nlohmann::json settings = readJson(settings_file);
    check(settings.contains("window_width"), "closing the app saves settings");
    check(settings.value("volume", -1) == 40, "closing the app saves the volume");
    check(settings.value("speed", 0.0) == 1.5, "closing the app saves the speed");
    settings["window_x"] = 50;
    settings["window_y"] = 60;
    settings["window_width"] = 1000;
    settings["window_height"] = 700;
    settings["maximized"] = false;
    nlohmann::json library = nlohmann::json::parse(std::ifstream(library_file), nullptr, false);
    check(library.is_array() && !library.empty(), "the library holds the clip before the second run");
    if (library.is_array() && !library.empty()) {
        library[0]["marks"].push_back({{"time", 18}, {"label", "Pomodoro 1"}, {"chapter", true}, {"note", ""}});
        library[0]["marks"].push_back({{"time", 24}, {"label", "Break 1"}, {"chapter", true}, {"note", ""}});
        library[0]["marks"].push_back({{"time", 34}, {"label", "Pomodoro 2"}, {"chapter", true}, {"note", ""}});
        std::ofstream(library_file) << library.dump();
    }

    settings["backend"] = "mpv";
    settings["pitch"] = 2;
    settings["keep_pitch"] = false;
    std::ofstream(settings_file) << settings.dump();

    secondRun(command_line, environment, library_file);

    nlohmann::json after = readJson(settings_file);
    check(after.value("window_x", -1) == 50 && after.value("window_width", 0) == 1000, "settings are saved again on close");
    check(after.value("silence_work", false), "closing the app saves the silent study setting");

    fs::remove_all(root, error);
    return report();
}
