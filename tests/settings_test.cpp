#include <climits>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <windows.h>

#include <nlohmann/json.hpp>

#include "check.h"
#include "settings.h"

namespace fs = std::filesystem;
using nlohmann::json;

static fs::path root;

static const std::vector<std::string> field_keys = {
    "backend", "videos_folder", "window_x", "window_y", "window_width",
    "window_height", "maximized", "volume", "speed", "pitch", "keep_pitch",
    "silence_work"};

static void writeFile(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
}

static std::string readFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static std::string formatDouble(double value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    return buffer;
}

static std::string describe(const Settings& settings)
{
    return "{backend=\"" + settings.backend + "\" videos_folder=\"" + settings.videos_folder
         + "\" window_x=" + std::to_string(settings.window_x)
         + " window_y=" + std::to_string(settings.window_y)
         + " window_width=" + std::to_string(settings.window_width)
         + " window_height=" + std::to_string(settings.window_height)
         + " maximized=" + std::string(settings.maximized ? "true" : "false")
         + " volume=" + std::to_string(settings.volume)
         + " speed=" + formatDouble(settings.speed)
         + " pitch=" + std::to_string(settings.pitch)
         + " keep_pitch=" + std::string(settings.keep_pitch ? "true" : "false")
         + " silence_work=" + std::string(settings.silence_work ? "true" : "false") + "}";
}

static bool sameSettings(const Settings& a, const Settings& b)
{
    return a.backend == b.backend && a.videos_folder == b.videos_folder
        && a.window_x == b.window_x && a.window_y == b.window_y
        && a.window_width == b.window_width && a.window_height == b.window_height
        && a.maximized == b.maximized && a.volume == b.volume && a.speed == b.speed
        && a.pitch == b.pitch && a.keep_pitch == b.keep_pitch && a.silence_work == b.silence_work;
}

static Settings tryLoad(const fs::path& file, const std::string& name)
{
    try {
        return loadSettings(file);
    } catch (...) {
        check(false, "loadSettings() does not throw: " + name);
        return Settings();
    }
}

static bool trySave(const Settings& settings, const fs::path& file, const std::string& name)
{
    try {
        return saveSettings(settings, file);
    } catch (...) {
        check(false, "saveSettings() does not throw: " + name);
        return false;
    }
}

static json baseJson()
{
    json document = json::object();
    document["backend"] = "mpv";
    document["videos_folder"] = "D:\\Clips\\Saved";
    document["window_x"] = 50;
    document["window_y"] = 60;
    document["window_width"] = 1600;
    document["window_height"] = 900;
    document["maximized"] = true;
    document["volume"] = 42;
    document["speed"] = 1.5;
    document["pitch"] = 3;
    document["keep_pitch"] = false;
    document["silence_work"] = true;
    return document;
}

static Settings baseSettings()
{
    Settings settings;
    settings.backend = "mpv";
    settings.videos_folder = "D:\\Clips\\Saved";
    settings.window_x = 50;
    settings.window_y = 60;
    settings.window_width = 1600;
    settings.window_height = 900;
    settings.maximized = true;
    settings.volume = 42;
    settings.speed = 1.5;
    settings.pitch = 3;
    settings.keep_pitch = false;
    settings.silence_work = true;
    return settings;
}

static void resetField(Settings& settings, const std::string& key)
{
    Settings defaults;
    if (key == "backend")
        settings.backend = defaults.backend;
    else if (key == "videos_folder")
        settings.videos_folder = defaults.videos_folder;
    else if (key == "window_x")
        settings.window_x = defaults.window_x;
    else if (key == "window_y")
        settings.window_y = defaults.window_y;
    else if (key == "window_width")
        settings.window_width = defaults.window_width;
    else if (key == "window_height")
        settings.window_height = defaults.window_height;
    else if (key == "maximized")
        settings.maximized = defaults.maximized;
    else if (key == "volume")
        settings.volume = defaults.volume;
    else if (key == "speed")
        settings.speed = defaults.speed;
    else if (key == "pitch")
        settings.pitch = defaults.pitch;
    else if (key == "keep_pitch")
        settings.keep_pitch = defaults.keep_pitch;
    else if (key == "silence_work")
        settings.silence_work = defaults.silence_work;
}

static Settings loadText(const std::string& content, const std::string& name)
{
    fs::path file = root / "cases" / "settings.json";
    writeFile(file, content);
    return tryLoad(file, name);
}

static void checkLoads(const std::string& content, const Settings& expected, const std::string& name)
{
    Settings loaded = loadText(content, name);
    check(sameSettings(loaded, expected), name + ", got " + describe(loaded));
}

static void checkBaseWith(const std::string& key, const json& value, const Settings& expected, const std::string& name)
{
    json document = baseJson();
    document[key] = value;
    checkLoads(document.dump(), expected, name);
}

static void checkBackend(const std::string& given, const std::string& wanted)
{
    Settings expected = baseSettings();
    expected.backend = wanted;
    checkBaseWith("backend", given, expected, "backend \"" + given + "\" loads as \"" + wanted + "\"");
}

static void checkVolume(int given, int wanted)
{
    Settings expected = baseSettings();
    expected.volume = wanted;
    checkBaseWith("volume", given, expected, "volume " + std::to_string(given) + " loads as " + std::to_string(wanted));
}

static void checkSpeed(double given, double wanted)
{
    Settings expected = baseSettings();
    expected.speed = wanted;
    checkBaseWith("speed", given, expected, "speed " + formatDouble(given) + " loads as " + formatDouble(wanted));
}

static void checkPitch(int given, int wanted)
{
    Settings expected = baseSettings();
    expected.pitch = wanted;
    checkBaseWith("pitch", given, expected, "pitch " + std::to_string(given) + " loads as " + std::to_string(wanted));
}

static void checkKeepPitch(bool given, bool wanted)
{
    Settings expected = baseSettings();
    expected.keep_pitch = wanted;
    checkBaseWith("keep_pitch", given, expected,
                  "keep_pitch " + std::string(given ? "true" : "false") + " loads as " + std::string(wanted ? "true" : "false"));
}

static void checkSilenceWork(bool given, bool wanted)
{
    Settings expected = baseSettings();
    expected.silence_work = wanted;
    checkBaseWith("silence_work", given, expected,
                  "silence_work " + std::string(given ? "true" : "false") + " loads as " + std::string(wanted ? "true" : "false"));
}

static void checkWidth(int given, int wanted)
{
    Settings expected = baseSettings();
    expected.window_width = wanted;
    checkBaseWith("window_width", given, expected, "window_width " + std::to_string(given) + " loads as " + std::to_string(wanted));
}

static void checkHeight(int given, int wanted)
{
    Settings expected = baseSettings();
    expected.window_height = wanted;
    checkBaseWith("window_height", given, expected, "window_height " + std::to_string(given) + " loads as " + std::to_string(wanted));
}

static void checkPosition(int x, int y)
{
    json document = baseJson();
    document["window_x"] = x;
    document["window_y"] = y;
    Settings expected = baseSettings();
    expected.window_x = x;
    expected.window_y = y;
    checkLoads(document.dump(), expected, "window position " + std::to_string(x) + "," + std::to_string(y) + " loads unchanged");
}

static void checkRoundTrip(const Settings& settings, const std::string& name)
{
    fs::path file = root / "roundtrip" / (name + ".json");
    check(trySave(settings, file, "round trip " + name), "saveSettings() returns true for " + name);
    check(fs::is_regular_file(file), "saveSettings() writes a file for " + name);
    Settings loaded = tryLoad(file, "round trip " + name);
    check(sameSettings(loaded, settings), "round trip keeps every field for " + name + ", saved " + describe(settings) + ", got " + describe(loaded));
}

static void testDefaults()
{
    Settings settings;
    check(settings.backend == "vlc", "default backend is vlc, got " + settings.backend);
    check(settings.videos_folder.empty(), "default videos_folder is empty, got " + settings.videos_folder);
    check(settings.window_x == -1 && settings.window_y == -1, "default window position is -1,-1");
    check(settings.window_width == 1280 && settings.window_height == 760, "default window size is 1280x760");
    check(!settings.maximized, "default maximized is false");
    check(settings.volume == 100, "default volume is 100, got " + std::to_string(settings.volume));
    check(settings.speed == 1.0, "default speed is 1.0, got " + formatDouble(settings.speed));
    check(settings.pitch == 0, "default pitch is 0, got " + std::to_string(settings.pitch));
    check(settings.keep_pitch, "default keep_pitch is true");
    check(!settings.silence_work, "default silence_work is false");
}

static void testMissingFile()
{
    Settings loaded = tryLoad(root / "missing" / "settings.json", "missing file");
    check(sameSettings(loaded, Settings()), "a missing file gives default settings, got " + describe(loaded));

    loaded = tryLoad(root / "missing-folder" / "deeper" / "settings.json", "missing folder");
    check(sameSettings(loaded, Settings()), "a file in a missing folder gives default settings, got " + describe(loaded));

    fs::path folder = root / "folder-instead-of-file";
    fs::create_directories(folder);
    loaded = tryLoad(folder, "folder path");
    check(sameSettings(loaded, Settings()), "loading a path that is a folder gives default settings, got " + describe(loaded));
}

static void testInvalidFiles()
{
    std::vector<std::pair<std::string, std::string>> cases = {
        {"an empty file", ""},
        {"a whitespace-only file", "  \r\n\t "},
        {"corrupt JSON", "{not json"},
        {"truncated JSON", "{\"backend\": \"mpv\", \"volume\": "},
        {"JSON with trailing garbage", "{\"backend\": \"mpv\", \"volume\": 5} trailing"},
        {"binary garbage", std::string("\xFF\xFE\x00\x01garbage", 11)},
        {"an empty JSON array", "[]"},
        {"a JSON array holding an object", "[{\"backend\": \"mpv\", \"volume\": 5}]"},
        {"a JSON integer", "42"},
        {"a JSON float", "1.5"},
        {"a JSON string", "\"mpv\""},
        {"JSON null", "null"},
        {"JSON true", "true"},
        {"JSON false", "false"},
        {"an empty JSON object", "{}"},
        {"an object with only unknown keys", "{\"theme\": \"dark\", \"Volume\": 5}"},
    };
    for (const auto& [name, content] : cases)
        checkLoads(content, Settings(), name + " gives default settings");
}

static void testMissingFields()
{
    for (const std::string& key : field_keys) {
        json document = baseJson();
        document.erase(key);
        Settings expected = baseSettings();
        resetField(expected, key);
        checkLoads(document.dump(), expected, "missing " + key + " keeps its default and the other fields load");
    }
}

static void testWrongTypes()
{
    std::vector<std::pair<std::string, json>> cases = {
        {"backend", 7},
        {"backend", true},
        {"backend", nullptr},
        {"backend", json::array({"mpv"})},
        {"backend", json::object({{"name", "mpv"}})},
        {"videos_folder", 7},
        {"videos_folder", false},
        {"videos_folder", nullptr},
        {"videos_folder", json::array({"C:\\Videos"})},
        {"videos_folder", json::object()},
        {"maximized", "true"},
        {"maximized", nullptr},
        {"maximized", json::array({true})},
        {"maximized", json::object()},
        {"speed", "1.25"},
        {"speed", true},
        {"speed", nullptr},
        {"speed", json::array({1.25})},
        {"speed", json::object()},
        {"pitch", "3"},
        {"pitch", true},
        {"pitch", false},
        {"pitch", nullptr},
        {"pitch", 1.5},
        {"pitch", -1.5},
        {"pitch", json::array({3})},
        {"pitch", json::object()},
        {"keep_pitch", 1},
        {"keep_pitch", 0},
        {"keep_pitch", 1.5},
        {"keep_pitch", "true"},
        {"keep_pitch", "false"},
        {"keep_pitch", nullptr},
        {"keep_pitch", json::array({false})},
        {"keep_pitch", json::object()},
        {"silence_work", 1},
        {"silence_work", 0},
        {"silence_work", -1},
        {"silence_work", 1.5},
        {"silence_work", "true"},
        {"silence_work", "false"},
        {"silence_work", ""},
        {"silence_work", nullptr},
        {"silence_work", json::array({true})},
        {"silence_work", json::array()},
        {"silence_work", json::object()},
        {"silence_work", json::object({{"silence_work", true}})},
    };
    for (const char* key : {"window_x", "window_y", "window_width", "window_height", "volume"}) {
        cases.push_back({key, "500"});
        cases.push_back({key, true});
        cases.push_back({key, false});
        cases.push_back({key, nullptr});
        cases.push_back({key, json::array({500})});
        cases.push_back({key, json::object()});
    }
    for (const auto& [key, value] : cases) {
        Settings expected = baseSettings();
        resetField(expected, key);
        checkBaseWith(key, value, expected, key + " given as " + value.dump() + " keeps its default and the other fields load");
    }

    json all_wrong = json::object();
    all_wrong["backend"] = 1;
    all_wrong["videos_folder"] = true;
    all_wrong["window_x"] = "10";
    all_wrong["window_y"] = nullptr;
    all_wrong["window_width"] = json::array();
    all_wrong["window_height"] = json::object();
    all_wrong["maximized"] = "yes";
    all_wrong["volume"] = "50";
    all_wrong["speed"] = "fast";
    all_wrong["pitch"] = "3";
    all_wrong["keep_pitch"] = 1;
    all_wrong["silence_work"] = "yes";
    checkLoads(all_wrong.dump(), Settings(), "every field with the wrong type gives default settings");
}

static void testBackend()
{
    checkBackend("vlc", "vlc");
    checkBackend("mpv", "mpv");
    checkBackend("VLC", "vlc");
    checkBackend("Mpv", "vlc");
    checkBackend("MPV", "vlc");
    checkBackend("", "vlc");
    checkBackend("mplayer", "vlc");
    checkBackend(" mpv", "vlc");
    checkBackend("mpv ", "vlc");
}

static void testVolume()
{
    checkVolume(-5, 0);
    checkVolume(-1, 0);
    checkVolume(0, 0);
    checkVolume(1, 1);
    checkVolume(50, 50);
    checkVolume(99, 99);
    checkVolume(100, 100);
    checkVolume(101, 100);
    checkVolume(1000000, 100);
    checkVolume(INT_MAX, 100);
    checkVolume(INT_MIN, 0);
}

static void testSpeed()
{
    checkSpeed(0.1, 0.5);
    checkSpeed(0.0, 0.5);
    checkSpeed(-1.5, 0.5);
    checkSpeed(0.5, 0.5);
    checkSpeed(0.75, 0.75);
    checkSpeed(1.0, 1.0);
    checkSpeed(1.25, 1.25);
    checkSpeed(2.0, 2.0);
    checkSpeed(2.5, 2.0);
    checkSpeed(3.0, 2.0);
    checkSpeed(1e300, 2.0);
}

static void testWindowSize()
{
    checkWidth(319, 1280);
    checkWidth(100, 1280);
    checkWidth(0, 1280);
    checkWidth(-800, 1280);
    checkWidth(INT_MIN, 1280);
    checkWidth(640, 640);
    checkWidth(1024, 1024);
    checkWidth(1920, 1920);
    checkWidth(3840, 3840);

    checkHeight(239, 760);
    checkHeight(100, 760);
    checkHeight(0, 760);
    checkHeight(-1, 760);
    checkHeight(INT_MIN, 760);
    checkHeight(480, 480);
    checkHeight(768, 768);
    checkHeight(1080, 1080);
    checkHeight(2160, 2160);

    json document = baseJson();
    document["window_width"] = 319;
    document["window_height"] = 239;
    Settings expected = baseSettings();
    expected.window_width = 1280;
    expected.window_height = 760;
    checkLoads(document.dump(), expected, "width 319 and height 239 together load as 1280x760");
}

static void testWindowPosition()
{
    checkPosition(-1, -1);
    checkPosition(0, 0);
    checkPosition(-1920, -1080);
    checkPosition(-5, 300);
    checkPosition(2560, -200);
    checkPosition(-32000, -32000);
    checkPosition(INT_MIN, INT_MAX);
}

static void testUtf8Folder()
{
    std::string raw = "{\"videos_folder\": \"D:\\\\Vid\xC3\xA9os\\\\\xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80\"}";
    Settings expected;
    expected.videos_folder = "D:\\Vid\xC3\xA9os\\\xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80";
    checkLoads(raw, expected, "raw UTF-8 videos_folder loads byte-exact");

    std::string escaped = "{\"videos_folder\": \"caf\\u00e9 \\u65e5\\u672c \\ud83d\\ude00\"}";
    expected.videos_folder = "caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80";
    checkLoads(escaped, expected, "\\u-escaped videos_folder loads as UTF-8");

    checkLoads("{\"videos_folder\": \"\"}", Settings(), "empty videos_folder loads as empty");

    Settings settings;
    settings.videos_folder = "C:\\Users\\Test\\Vid\xC3\xA9os \xE6\x97\xA5\xE6\x9C\xAC\\\xF0\x9F\x8E\xAC Clips";
    checkRoundTrip(settings, "utf8-folder");
}

static void testExtraKeys()
{
    json document = baseJson();
    document["theme"] = "dark";
    document["recent"] = json::array({1, 2, 3});
    document["nested"] = json::object({{"volume", 5}, {"backend", "vlc"}});
    document["Volume"] = 7;
    document["BACKEND"] = "vlc";
    document["window"] = nullptr;
    checkLoads(document.dump(), baseSettings(), "unknown extra keys are ignored");
}

static void testRoundTrips()
{
    checkRoundTrip(Settings(), "defaults");
    checkRoundTrip(baseSettings(), "base");

    Settings low;
    low.backend = "vlc";
    low.videos_folder = "";
    low.window_x = -1920;
    low.window_y = -1080;
    low.window_width = 640;
    low.window_height = 480;
    low.maximized = false;
    low.volume = 0;
    low.speed = 0.5;
    checkRoundTrip(low, "low");

    Settings high;
    high.backend = "mpv";
    high.videos_folder = "\\\\server\\share\\Vid\xC3\xA9os";
    high.window_x = INT_MAX;
    high.window_y = INT_MIN;
    high.window_width = 7680;
    high.window_height = 4320;
    high.maximized = true;
    high.volume = 100;
    high.speed = 2.0;
    checkRoundTrip(high, "high");

    Settings odd;
    odd.backend = "mpv";
    odd.videos_folder = "C:\\Path with \"quotes\"/slashes\\and\ttab\nnewline";
    odd.window_x = 0;
    odd.window_y = -7;
    odd.window_width = 1281;
    odd.window_height = 761;
    odd.maximized = true;
    odd.volume = 37;
    odd.speed = 1.25;
    checkRoundTrip(odd, "odd");

    Settings fractional = baseSettings();
    fractional.speed = 0.75;
    checkRoundTrip(fractional, "speed-0.75");

    fs::path file = root / "roundtrip" / "base.json";
    json document;
    bool parsed = false;
    try {
        document = json::parse(readFile(file));
        parsed = true;
    } catch (...) {
    }
    check(parsed, "saved settings file is valid JSON");
    check(document.is_object(), "saved settings file is a JSON object");
    if (document.is_object()) {
        Settings settings = baseSettings();
        for (const std::string& key : field_keys)
            check(document.contains(key), "saved settings file has the key " + key);
        check(document.value("backend", json()) == json(settings.backend), "saved backend value is stored under backend");
        check(document.value("videos_folder", json()) == json(settings.videos_folder), "saved videos_folder value is stored under videos_folder");
        check(document.value("window_x", json()) == json(settings.window_x), "saved window_x value is stored under window_x");
        check(document.value("window_y", json()) == json(settings.window_y), "saved window_y value is stored under window_y");
        check(document.value("window_width", json()) == json(settings.window_width), "saved window_width value is stored under window_width");
        check(document.value("window_height", json()) == json(settings.window_height), "saved window_height value is stored under window_height");
        check(document.value("maximized", json()) == json(settings.maximized), "saved maximized value is stored under maximized");
        check(document.value("volume", json()) == json(settings.volume), "saved volume value is stored under volume");
        check(document.value("speed", json()) == json(settings.speed), "saved speed value is stored under speed");
        check(document.value("pitch", json()) == json(settings.pitch), "saved pitch value is stored under pitch");
        check(document.value("keep_pitch", json()) == json(settings.keep_pitch), "saved keep_pitch value is stored under keep_pitch");
        check(document.value("silence_work", json()) == json(settings.silence_work), "saved silence_work value is stored under silence_work");
    }

    json clamped = baseJson();
    clamped["backend"] = "MPV";
    clamped["volume"] = 250;
    clamped["speed"] = 9.0;
    clamped["window_width"] = 10;
    clamped["pitch"] = 50;
    Settings loaded = loadText(clamped.dump(), "clamped then saved");
    checkRoundTrip(loaded, "loaded-from-clamped");
}

static void testNestedFolders()
{
    fs::path file = root / "nested" / "a" / "b" / "c" / "settings.json";
    Settings settings = baseSettings();
    check(trySave(settings, file, "nested folders"), "saveSettings() into missing nested folders returns true");
    check(fs::is_regular_file(file), "saveSettings() into missing nested folders writes the file");
    Settings loaded = tryLoad(file, "nested folders");
    check(sameSettings(loaded, settings), "settings saved into new nested folders load back, got " + describe(loaded));
}

static void testOverwrite()
{
    fs::path file = root / "overwrite" / "settings.json";
    Settings first = baseSettings();
    first.videos_folder = std::string(2000, 'x');
    trySave(first, file, "overwrite first");

    Settings second;
    second.volume = 5;
    check(trySave(second, file, "overwrite second"), "saveSettings() over an existing file returns true");
    Settings loaded = tryLoad(file, "overwrite");
    check(sameSettings(loaded, second), "saveSettings() replaces the previous settings, got " + describe(loaded));

    bool valid = false;
    try {
        valid = json::parse(readFile(file)).is_object();
    } catch (...) {
    }
    check(valid, "a shorter save over a longer file leaves a valid JSON object");

    writeFile(file, "{not json");
    check(trySave(first, file, "overwrite corrupt"), "saveSettings() over a corrupt file returns true");
    loaded = tryLoad(file, "overwrite corrupt");
    check(sameSettings(loaded, first), "a corrupt file is replaced by a good save, got " + describe(loaded));
}

static void testSaveFailure()
{
    fs::path blocker = root / "blocker";
    writeFile(blocker, "this is a file, not a folder");
    Settings settings = baseSettings();
    check(!trySave(settings, blocker / "settings.json", "parent is a file"), "saveSettings() returns false when the parent path is a file");
    check(fs::is_regular_file(blocker), "the blocking file is still a file after a failed save");
    check(readFile(blocker) == "this is a file, not a folder", "failed saveSettings() leaves the blocking file unchanged, got " + readFile(blocker));

    check(!trySave(settings, blocker / "deeper" / "settings.json", "ancestor is a file"), "saveSettings() returns false when an ancestor path is a file");
    check(readFile(blocker) == "this is a file, not a folder", "failed deeper saveSettings() leaves the blocking file unchanged");

    fs::path folder = root / "target-is-folder";
    writeFile(folder / "keep.txt", "keep");
    check(!trySave(settings, folder, "target is a folder"), "saveSettings() returns false when the target path is a folder");
    check(fs::is_directory(folder) && readFile(folder / "keep.txt") == "keep", "failed saveSettings() onto a folder leaves the folder and its contents alone");

    fs::path file = root / "locked" / "settings.json";
    Settings good = baseSettings();
    trySave(good, file, "locked setup");
    std::string before = readFile(file);
    HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(handle != INVALID_HANDLE_VALUE, "test setup: settings file can be locked");
    if (handle != INVALID_HANDLE_VALUE) {
        Settings loaded = tryLoad(file, "locked load");
        check(sameSettings(loaded, Settings()), "loading an unreadable locked file gives default settings, got " + describe(loaded));
        Settings other;
        other.volume = 1;
        check(!trySave(other, file, "locked save"), "saveSettings() returns false when the file is locked by another handle");
        CloseHandle(handle);
    }
    check(readFile(file) == before, "failed saveSettings() does not change the existing good file");
    Settings loaded = tryLoad(file, "after locked save");
    check(sameSettings(loaded, good), "existing good file still loads after a failed save, got " + describe(loaded));
}

static void testSaveTwice()
{
    fs::path file = root / "twice" / "settings.json";
    Settings settings = baseSettings();
    check(trySave(settings, file, "save twice first"), "first saveSettings() returns true");
    check(trySave(settings, file, "save twice second"), "second saveSettings() of the same settings returns true");
    Settings loaded = tryLoad(file, "save twice");
    check(sameSettings(loaded, settings), "settings saved twice load back, got " + describe(loaded));

    Settings changed = settings;
    changed.maximized = false;
    changed.speed = 0.5;
    check(trySave(changed, file, "save twice changed"), "saving changed settings after two saves returns true");
    loaded = tryLoad(file, "save twice changed");
    check(sameSettings(loaded, changed), "the last save wins, got " + describe(loaded));
}

static void testLoadTwice()
{
    fs::path file = root / "load-twice" / "settings.json";
    Settings settings = baseSettings();
    trySave(settings, file, "load twice setup");
    Settings first = tryLoad(file, "load twice first");
    Settings second = tryLoad(file, "load twice second");
    check(sameSettings(first, settings), "first load gives the saved settings, got " + describe(first));
    check(sameSettings(second, settings), "second load of the same file gives the saved settings, got " + describe(second));

    fs::path hand = root / "load-twice" / "hand.json";
    writeFile(hand, "{\"backend\": \"VLC\", \"volume\": 101, \"speed\": 3.0, \"window_width\": 319, \"window_x\": -300}");
    Settings expected;
    expected.volume = 100;
    expected.speed = 2.0;
    expected.window_x = -300;
    Settings hand_first = tryLoad(hand, "hand-edited first");
    Settings hand_second = tryLoad(hand, "hand-edited second");
    check(sameSettings(hand_first, expected), "hand-edited file loads with values fixed, got " + describe(hand_first));
    check(sameSettings(hand_second, expected), "loading the hand-edited file again gives the same result, got " + describe(hand_second));
}

static void testPitch()
{
    checkPitch(0, 0);
    checkPitch(1, 1);
    checkPitch(-1, -1);
    checkPitch(5, 5);
    checkPitch(-5, -5);
    checkPitch(6, 6);
    checkPitch(-6, -6);
    checkPitch(7, 6);
    checkPitch(-7, -6);
    checkPitch(12, 6);
    checkPitch(-12, -6);
    checkPitch(1000000, 6);
    checkPitch(INT_MAX, 6);
    checkPitch(INT_MIN, -6);
}

static void testKeepPitch()
{
    checkKeepPitch(true, true);
    checkKeepPitch(false, false);

    Settings alone;
    alone.keep_pitch = false;
    checkLoads("{\"keep_pitch\": false}", alone, "keep_pitch false on its own loads as false and the other fields default");
}

static void testSilenceWork()
{
    checkSilenceWork(true, true);
    checkSilenceWork(false, false);

    Settings alone;
    alone.silence_work = true;
    checkLoads("{\"silence_work\": true}", alone, "silence_work true on its own loads as true and the other fields default");

    Settings off;
    checkLoads("{\"silence_work\": false}", off, "silence_work false on its own loads as false and the other fields default");

    checkLoads("{\"Silence_Work\": true}", Settings(), "silence_work is read case-sensitively");
    checkLoads("{\"silencework\": true}", Settings(), "a key without the underscore is not silence_work");

    json without = baseJson();
    without.erase("silence_work");
    Settings expected = baseSettings();
    expected.silence_work = false;
    checkLoads(without.dump(), expected, "a missing silence_work falls back to false and the other fields load");
}

static void testSilenceWorkRoundTrips()
{
    for (bool silence_work : {true, false}) {
        Settings settings = baseSettings();
        settings.silence_work = silence_work;
        checkRoundTrip(settings, "silence-" + std::string(silence_work ? "true" : "false"));
    }

    Settings on_defaults;
    on_defaults.silence_work = true;
    checkRoundTrip(on_defaults, "silence-true-otherwise-default");

    for (bool silence_work : {true, false}) {
        for (bool keep_pitch : {true, false}) {
            for (bool maximized : {true, false}) {
                Settings settings = baseSettings();
                settings.silence_work = silence_work;
                settings.keep_pitch = keep_pitch;
                settings.maximized = maximized;
                checkRoundTrip(settings, "flags-" + std::string(silence_work ? "s" : "-") + std::string(keep_pitch ? "k" : "-")
                                             + std::string(maximized ? "m" : "-"));
            }
        }
    }

    fs::path file = root / "silence-only" / "settings.json";
    Settings saved = baseSettings();
    saved.silence_work = true;
    check(trySave(saved, file, "silence_work save"), "saveSettings() with silence_work true returns true");
    json document;
    bool parsed = false;
    try {
        document = json::parse(readFile(file));
        parsed = true;
    } catch (...) {
    }
    check(parsed && document.is_object(), "the file saved with silence_work true is a valid JSON object");
    if (parsed && document.is_object()) {
        check(document.contains("silence_work"), "the saved file has the silence_work key");
        check(document.value("silence_work", json()) == json(true),
              "the saved silence_work is the boolean true, got " + document.value("silence_work", json()).dump());
    }

    saved.silence_work = false;
    check(trySave(saved, file, "silence_work off save"), "saveSettings() with silence_work false returns true");
    parsed = false;
    try {
        document = json::parse(readFile(file));
        parsed = true;
    } catch (...) {
    }
    check(parsed && document.is_object(), "the file saved with silence_work false is a valid JSON object");
    if (parsed && document.is_object())
        check(document.value("silence_work", json()) == json(false),
              "the saved silence_work is the boolean false, got " + document.value("silence_work", json()).dump());
    Settings loaded = tryLoad(file, "silence_work off load");
    check(sameSettings(loaded, saved), "silence_work false round trips with every other field, got " + describe(loaded));
}

static void testSettingsFileWithoutSilenceWork()
{
    fs::path file = root / "no-silence" / "settings.json";
    Settings settings = baseSettings();
    settings.silence_work = true;
    check(trySave(settings, file, "no-silence setup"), "saveSettings() for the no-silence setup returns true");

    json document;
    bool parsed = false;
    try {
        document = json::parse(readFile(file));
        parsed = true;
    } catch (...) {
    }
    check(parsed, "the file used to build a settings file without silence_work is valid JSON");
    if (!parsed || !document.is_object())
        return;

    document.erase("silence_work");
    check(!document.contains("silence_work"), "the older settings file has no silence_work key");
    writeFile(file, document.dump());

    Settings expected = baseSettings();
    expected.silence_work = false;
    Settings loaded = tryLoad(file, "settings file without silence_work");
    check(sameSettings(loaded, expected), "a settings file without silence_work loads as false and keeps its other values, got " + describe(loaded));

    check(trySave(loaded, file, "no-silence resave"), "saving a settings file loaded without silence_work returns true");
    Settings again = tryLoad(file, "settings file without silence_work resaved");
    check(sameSettings(again, expected), "the resaved settings file loads the same, got " + describe(again));

    check(trySave(settings, file, "no-silence reenable"), "saving with silence_work back on returns true");
    Settings back = tryLoad(file, "silence_work back on");
    check(sameSettings(back, settings), "turning silence_work back on survives a save and load, got " + describe(back));
}

static void testPitchRoundTrips()
{
    for (int pitch : {-6, -1, 0, 1, 6}) {
        for (bool keep_pitch : {true, false}) {
            Settings settings = baseSettings();
            settings.pitch = pitch;
            settings.keep_pitch = keep_pitch;
            checkRoundTrip(settings, "pitch-" + std::to_string(pitch) + "-keep-" + std::string(keep_pitch ? "true" : "false"));
        }
    }
}

static void testOldSettingsFile()
{
    fs::path file = root / "old-format" / "settings.json";
    Settings settings = baseSettings();
    check(trySave(settings, file, "old format setup"), "saveSettings() for the old-format setup returns true");

    json document;
    bool parsed = false;
    try {
        document = json::parse(readFile(file));
        parsed = true;
    } catch (...) {
    }
    check(parsed, "the saved file used to build an old settings file is valid JSON");
    if (!parsed || !document.is_object())
        return;

    document.erase("pitch");
    document.erase("keep_pitch");
    check(!document.contains("pitch") && !document.contains("keep_pitch"), "the old settings file has neither new key");
    writeFile(file, document.dump());

    Settings expected = baseSettings();
    expected.pitch = 0;
    expected.keep_pitch = true;
    Settings loaded = tryLoad(file, "old settings file");
    check(sameSettings(loaded, expected), "a settings file without pitch and keep_pitch loads with 0 and true and keeps its other values, got " + describe(loaded));

    check(trySave(loaded, file, "old format resave"), "saving a settings file loaded from the old format returns true");
    Settings again = tryLoad(file, "old settings file resaved");
    check(sameSettings(again, expected), "the resaved old settings file loads the same, got " + describe(again));
}

int main()
{
    root = fs::temp_directory_path() / ("youtonomous-settings-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root);

    testDefaults();
    testMissingFile();
    testInvalidFiles();
    testMissingFields();
    testWrongTypes();
    testBackend();
    testVolume();
    testSpeed();
    testWindowSize();
    testWindowPosition();
    testUtf8Folder();
    testExtraKeys();
    testRoundTrips();
    testNestedFolders();
    testOverwrite();
    testSaveFailure();
    testSaveTwice();
    testLoadTwice();
    testPitch();
    testKeepPitch();
    testPitchRoundTrips();
    testSilenceWork();
    testSilenceWorkRoundTrips();
    testSettingsFileWithoutSilenceWork();
    testOldSettingsFile();

    fs::remove_all(root, error);
    return report();
}
