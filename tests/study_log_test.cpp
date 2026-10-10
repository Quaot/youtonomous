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
#include "study_log.h"

namespace fs = std::filesystem;
using nlohmann::json;

static fs::path root;

static const std::vector<std::string> field_keys = {
    "day", "video_id", "label", "seconds", "speed", "pitch"};

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

static StudyEntry makeEntry(const std::string& day, const std::string& video_id,
                            const std::string& label, int seconds, double speed, int pitch)
{
    StudyEntry entry;
    entry.day = day;
    entry.video_id = video_id;
    entry.label = label;
    entry.seconds = seconds;
    entry.speed = speed;
    entry.pitch = pitch;
    return entry;
}

static StudyEntry baseEntry()
{
    return makeEntry("2026-10-09", "dQw4w9WgXcQ", "Pomodoro 1", 1500, 1.25, -2);
}

static json entryJson(const StudyEntry& entry)
{
    json object = json::object();
    object["day"] = entry.day;
    object["video_id"] = entry.video_id;
    object["label"] = entry.label;
    object["seconds"] = entry.seconds;
    object["speed"] = entry.speed;
    object["pitch"] = entry.pitch;
    return object;
}

static json baseJson()
{
    return entryJson(baseEntry());
}

static std::string describe(const StudyEntry& entry)
{
    return "{day=\"" + entry.day + "\" video_id=\"" + entry.video_id + "\" label=\"" + entry.label
         + "\" seconds=" + std::to_string(entry.seconds)
         + " speed=" + formatDouble(entry.speed)
         + " pitch=" + std::to_string(entry.pitch) + "}";
}

static std::string describeAll(const std::vector<StudyEntry>& entries)
{
    std::string text = "[" + std::to_string(entries.size()) + ":";
    for (const StudyEntry& entry : entries)
        text += " " + describe(entry);
    return text + "]";
}

static bool sameEntry(const StudyEntry& a, const StudyEntry& b)
{
    return a.day == b.day && a.video_id == b.video_id && a.label == b.label
        && a.seconds == b.seconds && a.speed == b.speed && a.pitch == b.pitch;
}

static bool sameEntries(const std::vector<StudyEntry>& a, const std::vector<StudyEntry>& b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t index = 0; index < a.size(); ++index) {
        if (!sameEntry(a[index], b[index]))
            return false;
    }
    return true;
}

static std::vector<StudyEntry> tryLoad(const fs::path& file, const std::string& name)
{
    try {
        return loadStudy(file);
    } catch (...) {
        check(false, "loadStudy() does not throw: " + name);
        return std::vector<StudyEntry>();
    }
}

static bool tryAppend(const StudyEntry& entry, const fs::path& file, const std::string& name)
{
    try {
        return appendStudy(entry, file);
    } catch (...) {
        check(false, "appendStudy() does not throw: " + name);
        return false;
    }
}

static void resetField(StudyEntry& entry, const std::string& key)
{
    StudyEntry defaults;
    if (key == "day")
        entry.day = defaults.day;
    else if (key == "video_id")
        entry.video_id = defaults.video_id;
    else if (key == "label")
        entry.label = defaults.label;
    else if (key == "seconds")
        entry.seconds = defaults.seconds;
    else if (key == "speed")
        entry.speed = defaults.speed;
    else if (key == "pitch")
        entry.pitch = defaults.pitch;
}

static std::vector<StudyEntry> loadText(const std::string& content, const std::string& name)
{
    fs::path file = root / "cases" / "study.json";
    writeFile(file, content);
    return tryLoad(file, name);
}

static void checkLoads(const std::string& content, const std::vector<StudyEntry>& expected, const std::string& name)
{
    std::vector<StudyEntry> loaded = loadText(content, name);
    check(sameEntries(loaded, expected), name + ", got " + describeAll(loaded));
}

static void checkBaseWith(const std::string& key, const json& value, const StudyEntry& expected, const std::string& name)
{
    json document = baseJson();
    document[key] = value;
    checkLoads(json::array({document}).dump(), std::vector<StudyEntry>{expected}, name);
}

static void checkFileHolds(const fs::path& file, const std::vector<StudyEntry>& expected, const std::string& name)
{
    json document;
    bool parsed = false;
    try {
        document = json::parse(readFile(file));
        parsed = true;
    } catch (...) {
    }
    check(parsed, "the study file is valid JSON: " + name);
    if (!parsed)
        return;
    check(document.is_array(), "the study file holds a JSON array: " + name + ", got " + std::string(document.type_name()));
    if (!document.is_array())
        return;
    check(document.size() == expected.size(),
          "the study file holds " + std::to_string(expected.size()) + " elements: " + name
              + ", got " + std::to_string(document.size()));
    if (document.size() != expected.size())
        return;
    for (std::size_t index = 0; index < expected.size(); ++index) {
        const json& element = document[index];
        std::string where = name + " element " + std::to_string(index);
        check(element.is_object(), "study file element is a JSON object: " + where + ", got " + std::string(element.type_name()));
        if (!element.is_object())
            continue;
        json wanted = entryJson(expected[index]);
        for (const std::string& key : field_keys) {
            check(element.contains(key), "study file element has the key " + key + ": " + where);
            check(element.value(key, json()) == wanted[key],
                  "study file element stores " + wanted[key].dump() + " under " + key + ": " + where
                      + ", got " + element.value(key, json()).dump());
        }
    }
}

static void checkRoundTrip(const StudyEntry& entry, const std::string& name)
{
    fs::path file = root / "roundtrip" / (name + ".json");
    std::error_code error;
    fs::remove(file, error);
    check(tryAppend(entry, file, "round trip " + name), "appendStudy() returns true for " + name);
    check(fs::is_regular_file(file), "appendStudy() writes a file for " + name);
    std::vector<StudyEntry> loaded = tryLoad(file, "round trip " + name);
    check(loaded.size() == 1, "round trip gives one entry for " + name + ", got " + std::to_string(loaded.size()));
    if (loaded.size() == 1) {
        check(sameEntry(loaded[0], entry), "round trip keeps every field for " + name
                                               + ", wrote " + describe(entry) + ", got " + describe(loaded[0]));
    }
}

static void testAppendToMissingFile()
{
    fs::path file = root / "append-missing" / "study.json";
    StudyEntry entry = baseEntry();
    check(tryAppend(entry, file, "missing file"), "appendStudy() into a missing file returns true");
    check(fs::is_regular_file(file), "appendStudy() creates the missing file");
    std::vector<StudyEntry> loaded = tryLoad(file, "missing file");
    check(sameEntries(loaded, std::vector<StudyEntry>{entry}),
          "the entry appended to a missing file loads back, got " + describeAll(loaded));
    checkFileHolds(file, std::vector<StudyEntry>{entry}, "appended to a missing file");

    fs::path deep = root / "append-deep" / "a" / "b" / "c" / "study.json";
    check(tryAppend(entry, deep, "missing folders"), "appendStudy() into missing parent folders returns true");
    check(fs::is_regular_file(deep), "appendStudy() creates the missing parent folders");
    loaded = tryLoad(deep, "missing folders");
    check(sameEntries(loaded, std::vector<StudyEntry>{entry}),
          "the entry appended into new folders loads back, got " + describeAll(loaded));
}

static void testAppendManyInARow()
{
    fs::path file = root / "append-many" / "study.json";
    std::vector<StudyEntry> wanted;
    for (int index = 0; index < 6; ++index) {
        StudyEntry entry = makeEntry("2026-10-0" + std::to_string(index + 1),
                                     "video" + std::to_string(index),
                                     "Pomodoro " + std::to_string(index + 1),
                                     60 * (index + 1), 1.0 + 0.25 * index, index - 3);
        check(tryAppend(entry, file, "append number " + std::to_string(index + 1)),
              "appendStudy() number " + std::to_string(index + 1) + " returns true");
        wanted.push_back(entry);
        std::vector<StudyEntry> loaded = tryLoad(file, "after append number " + std::to_string(index + 1));
        check(sameEntries(loaded, wanted),
              "after append number " + std::to_string(index + 1) + " every earlier entry is still there in order, got "
                  + describeAll(loaded));
    }
    checkFileHolds(file, wanted, "six appends in a row");
}

static void testNonPositiveSeconds()
{
    int index = 0;
    for (int seconds : {0, -1, -1500}) {
        std::string label = "seconds " + std::to_string(seconds);
        fs::path file = root / ("skip-" + std::to_string(index++)) / "study.json";
        StudyEntry entry = baseEntry();
        entry.seconds = seconds;
        check(tryAppend(entry, file, label), "appendStudy() with " + label + " returns true");
        check(!fs::exists(file), "appendStudy() with " + label + " does not create the file");
        std::vector<StudyEntry> loaded = tryLoad(file, label);
        check(loaded.empty(), "nothing loads after appendStudy() with " + label + ", got " + describeAll(loaded));
    }

    fs::path file = root / "skip-existing" / "study.json";
    StudyEntry good = baseEntry();
    check(tryAppend(good, file, "skip setup"), "appendStudy() for the non-positive setup returns true");
    std::string before = readFile(file);
    for (int seconds : {0, -1, -90}) {
        std::string label = "seconds " + std::to_string(seconds);
        StudyEntry entry = makeEntry("2026-10-10", "other", "Pomodoro 2", seconds, 2.0, 4);
        check(tryAppend(entry, file, "existing file, " + label),
              "appendStudy() with " + label + " onto an existing file returns true");
        check(readFile(file) == before,
              "appendStudy() with " + label + " leaves the existing file byte-exact, got " + readFile(file));
    }
    std::vector<StudyEntry> loaded = tryLoad(file, "skip existing");
    check(sameEntries(loaded, std::vector<StudyEntry>{good}),
          "only the positive entry is in the file after skipped appends, got " + describeAll(loaded));
}

static void testAppendFailures()
{
    StudyEntry entry = baseEntry();

    fs::path blocker = root / "blocker";
    writeFile(blocker, "this is a file, not a folder");
    check(!tryAppend(entry, blocker / "study.json", "parent is a file"),
          "appendStudy() returns false when the parent path is an existing regular file");
    check(fs::is_regular_file(blocker), "the blocking file is still a file after a failed appendStudy()");
    check(readFile(blocker) == "this is a file, not a folder",
          "a failed appendStudy() leaves the blocking file unchanged, got " + readFile(blocker));

    check(!tryAppend(entry, blocker / "deeper" / "study.json", "ancestor is a file"),
          "appendStudy() returns false when an ancestor path is an existing regular file");
    check(readFile(blocker) == "this is a file, not a folder",
          "a failed deeper appendStudy() leaves the blocking file unchanged");

    fs::path folder = root / "target-is-folder";
    writeFile(folder / "keep.txt", "keep");
    check(!tryAppend(entry, folder, "target is a folder"),
          "appendStudy() returns false when the target path is a folder");
    check(fs::is_directory(folder), "the target folder is still a folder after a failed appendStudy()");
    check(readFile(folder / "keep.txt") == "keep",
          "a failed appendStudy() onto a folder leaves the folder contents alone, got " + readFile(folder / "keep.txt"));

    fs::path locked = root / "locked" / "study.json";
    check(tryAppend(entry, locked, "locked setup"), "appendStudy() for the locked setup returns true");
    std::string before = readFile(locked);
    HANDLE handle = CreateFileW(locked.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(handle != INVALID_HANDLE_VALUE, "test setup: the study file can be locked");
    if (handle != INVALID_HANDLE_VALUE) {
        std::vector<StudyEntry> loaded = tryLoad(locked, "locked load");
        check(loaded.empty(), "loading an unreadable locked file gives an empty vector, got " + describeAll(loaded));
        StudyEntry other = makeEntry("2026-10-11", "locked", "Pomodoro 9", 300, 1.0, 0);
        check(!tryAppend(other, locked, "locked append"),
              "appendStudy() returns false when the file is locked by another handle");
        CloseHandle(handle);
    }
    check(readFile(locked) == before,
          "a failed appendStudy() does not change the existing file, got " + readFile(locked));
    std::vector<StudyEntry> loaded = tryLoad(locked, "after locked append");
    check(sameEntries(loaded, std::vector<StudyEntry>{entry}),
          "the existing entry still loads after a failed appendStudy(), got " + describeAll(loaded));
}

static void testAppendOverCorrupt()
{
    std::vector<std::pair<std::string, std::string>> cases = {
        {"corrupt JSON", "{not json"},
        {"a truncated array", "[{\"day\": \"2026-10-01\", \"seconds\": "},
        {"an array with trailing garbage", "[{\"day\": \"2026-10-01\", \"seconds\": 60}] trailing"},
        {"plain text", "yesterday I studied"},
        {"binary garbage", std::string("\xFF\xFE\x00\x01garbage", 11)},
    };
    int index = 0;
    for (const auto& [name, content] : cases) {
        fs::path file = root / ("corrupt-" + std::to_string(index++)) / "study.json";
        writeFile(file, content);
        StudyEntry entry = baseEntry();
        check(tryAppend(entry, file, name), "appendStudy() over " + name + " returns true");
        std::vector<StudyEntry> loaded = tryLoad(file, name);
        check(sameEntries(loaded, std::vector<StudyEntry>{entry}),
              "appendStudy() over " + name + " leaves an array holding just this entry, got " + describeAll(loaded));
        checkFileHolds(file, std::vector<StudyEntry>{entry}, "appended over " + name);

        StudyEntry second = makeEntry("2026-10-12", "second", "Pomodoro 2", 900, 1.5, 3);
        check(tryAppend(second, file, name + " then another append"),
              "appendStudy() after recovering from " + name + " returns true");
        loaded = tryLoad(file, name + " then another append");
        check(sameEntries(loaded, std::vector<StudyEntry>{entry, second}),
              "appending again after recovering from " + name + " keeps both entries in order, got " + describeAll(loaded));
    }
}

static void testLoadMissingFile()
{
    std::vector<StudyEntry> loaded = tryLoad(root / "no-such" / "study.json", "missing file");
    check(loaded.empty(), "a missing file gives an empty vector, got " + describeAll(loaded));

    loaded = tryLoad(root / "no-such-folder" / "deeper" / "study.json", "missing folder");
    check(loaded.empty(), "a file in a missing folder gives an empty vector, got " + describeAll(loaded));

    fs::path folder = root / "load-folder";
    fs::create_directories(folder);
    loaded = tryLoad(folder, "folder path");
    check(loaded.empty(), "loading a path that is a folder gives an empty vector, got " + describeAll(loaded));
}

static void testLoadInvalidFiles()
{
    std::vector<std::pair<std::string, std::string>> cases = {
        {"an empty file", ""},
        {"a whitespace-only file", "  \r\n\t "},
        {"corrupt JSON", "{not json"},
        {"a truncated array", "[{\"day\": \"2026-10-01\", \"seconds\": "},
        {"an array with trailing garbage", "[{\"seconds\": 60}] trailing"},
        {"binary garbage", std::string("\xFF\xFE\x00\x01garbage", 11)},
        {"an empty JSON array", "[]"},
        {"a JSON object", "{\"day\": \"2026-10-01\", \"seconds\": 60}"},
        {"a JSON object holding an array", "{\"entries\": [{\"seconds\": 60}]}"},
        {"a JSON integer", "42"},
        {"a JSON float", "1.5"},
        {"a JSON string", "\"2026-10-01\""},
        {"JSON null", "null"},
        {"JSON true", "true"},
        {"JSON false", "false"},
        {"an array of numbers", "[1, 2, 3]"},
        {"an array of strings", "[\"a\", \"b\"]"},
        {"an array of nulls", "[null, null]"},
        {"an array of arrays", "[[], [{\"seconds\": 60}]]"},
        {"an array of empty arrays", "[[]]"},
    };
    for (const auto& [name, content] : cases)
        checkLoads(content, std::vector<StudyEntry>(), name + " gives an empty vector");
}

static void testLoadMixedContent()
{
    StudyEntry first = makeEntry("2026-10-01", "a", "Pomodoro 1", 60, 1.0, 0);
    StudyEntry second = makeEntry("2026-10-02", "b", "Pomodoro 2", 120, 1.5, 2);
    json document = json::array({entryJson(first), 7, entryJson(second)});
    checkLoads(document.dump(), std::vector<StudyEntry>{first, second},
               "a number between two objects is skipped and the objects load");

    document = json::array({"text", entryJson(first), nullptr, json::array({1}), entryJson(second), true});
    checkLoads(document.dump(), std::vector<StudyEntry>{first, second},
               "strings, nulls, arrays and booleans in the array are skipped");

    document = json::array({nullptr, entryJson(first)});
    checkLoads(document.dump(), std::vector<StudyEntry>{first},
               "a null before the only object is skipped");

    document = json::array({entryJson(first), json::object()});
    checkLoads(document.dump(), std::vector<StudyEntry>{first, StudyEntry()},
               "an empty object loads as an entry holding every default");
}

static void testMissingFields()
{
    for (const std::string& key : field_keys) {
        json document = baseJson();
        document.erase(key);
        StudyEntry expected = baseEntry();
        resetField(expected, key);
        checkLoads(json::array({document}).dump(), std::vector<StudyEntry>{expected},
                   "missing " + key + " takes its default and the other fields load");
    }

    json document = baseJson();
    for (const std::string& key : field_keys)
        document.erase(key);
    checkLoads(json::array({document}).dump(), std::vector<StudyEntry>{StudyEntry()},
               "an object with no known keys loads as every default");
}

static void testWrongTypes()
{
    std::vector<std::pair<std::string, json>> cases;
    for (const char* key : {"day", "video_id", "label"}) {
        cases.push_back({key, 7});
        cases.push_back({key, 1.5});
        cases.push_back({key, true});
        cases.push_back({key, false});
        cases.push_back({key, nullptr});
        cases.push_back({key, json::array({"2026-10-09"})});
        cases.push_back({key, json::object({{"text", "2026-10-09"}})});
    }
    for (const char* key : {"seconds", "pitch"}) {
        cases.push_back({key, "60"});
        cases.push_back({key, true});
        cases.push_back({key, false});
        cases.push_back({key, nullptr});
        cases.push_back({key, 2.5});
        cases.push_back({key, -2.5});
        cases.push_back({key, json::array({60})});
        cases.push_back({key, json::object()});
    }
    cases.push_back({"speed", "1.25"});
    cases.push_back({"speed", true});
    cases.push_back({"speed", false});
    cases.push_back({"speed", nullptr});
    cases.push_back({"speed", json::array({1.25})});
    cases.push_back({"speed", json::object()});

    for (const auto& [key, value] : cases) {
        StudyEntry expected = baseEntry();
        resetField(expected, key);
        checkBaseWith(key, value, expected,
                      key + " given as " + value.dump() + " takes its default and the other fields load");
    }

    json all_wrong = json::object();
    all_wrong["day"] = 1;
    all_wrong["video_id"] = true;
    all_wrong["label"] = nullptr;
    all_wrong["seconds"] = "600";
    all_wrong["speed"] = json::array();
    all_wrong["pitch"] = 1.5;
    checkLoads(json::array({all_wrong}).dump(), std::vector<StudyEntry>{StudyEntry()},
               "every field with the wrong type loads as every default");

    StudyEntry expected = baseEntry();
    expected.seconds = 0;
    json one_wrong = baseJson();
    one_wrong["seconds"] = 2.5;
    json document = json::array({one_wrong, baseJson()});
    checkLoads(document.dump(), std::vector<StudyEntry>{expected, baseEntry()},
               "a fractional seconds in one entry does not disturb the next entry");
}

static void testWholeNumbers()
{
    for (int seconds : {1, 60, 1500, 86400}) {
        StudyEntry expected = baseEntry();
        expected.seconds = seconds;
        checkBaseWith("seconds", seconds, expected,
                      "seconds " + std::to_string(seconds) + " loads unchanged");
    }
    for (int pitch : {-7, -1, 0, 1, 7}) {
        StudyEntry expected = baseEntry();
        expected.pitch = pitch;
        checkBaseWith("pitch", pitch, expected, "pitch " + std::to_string(pitch) + " loads unchanged");
    }
    for (double speed : {0.5, 0.75, 1.0, 1.25, 2.0, 3.5}) {
        StudyEntry expected = baseEntry();
        expected.speed = speed;
        checkBaseWith("speed", speed, expected, "speed " + formatDouble(speed) + " loads unchanged");
    }
    StudyEntry expected = baseEntry();
    expected.speed = 2.0;
    checkBaseWith("speed", 2, expected, "speed given as the whole number 2 loads as 2.0");
}

static void testUtf8Text()
{
    std::string raw = "[{\"day\": \"2026-10-09\", \"video_id\": \"id\", \"label\": " "\"Pomodoro \xE6\x97\xA5\xE6\x9C\xAC caf\xC3\xA9 \xF0\x9F\x8D\x85\", \"seconds\": 1500}]";
    StudyEntry expected = makeEntry("2026-10-09", "id", "Pomodoro \xE6\x97\xA5\xE6\x9C\xAC caf\xC3\xA9 \xF0\x9F\x8D\x85", 1500, 1.0, 0);
    checkLoads(raw, std::vector<StudyEntry>{expected}, "a raw UTF-8 label loads byte-exact");

    std::string escaped = "[{\"label\": \"caf\\u00e9 \\u65e5\\u672c \\ud83d\\ude00\", \"seconds\": 300}]";
    StudyEntry wanted = makeEntry("", "", "caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80", 300, 1.0, 0);
    checkLoads(escaped, std::vector<StudyEntry>{wanted}, "a \\u-escaped label loads as UTF-8");

    checkLoads("[{\"label\": \"\", \"seconds\": 60}]",
               std::vector<StudyEntry>{makeEntry("", "", "", 60, 1.0, 0)}, "an empty label loads as empty");

    checkRoundTrip(makeEntry("2026-10-09", "\xE6\x97\xA5\xE6\x9C\xAC-id",
                             "Pomodoro \xF0\x9F\x8D\x85 caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC", 1500, 1.25, -3),
                   "utf8-label");
    checkRoundTrip(makeEntry("2026-10-09", "id", "Label with \"quotes\"/slashes\\and\ttab\nnewline", 60, 1.0, 0),
                   "escaped-label");
}

static void testExtraKeys()
{
    json document = baseJson();
    document["note"] = "felt sharp";
    document["Seconds"] = 9999;
    document["DAY"] = "2000-01-01";
    document["blocks"] = json::array({1, 2, 3});
    document["nested"] = json::object({{"seconds", 42}, {"day", "1999-12-31"}});
    document["extra"] = nullptr;
    checkLoads(json::array({document}).dump(), std::vector<StudyEntry>{baseEntry()},
               "unknown extra keys are ignored");
}

static void testRoundTrips()
{
    checkRoundTrip(baseEntry(), "base");
    checkRoundTrip(makeEntry("2026-10-09", "id", "Pomodoro 1", 1, 1.0, 0), "one-second");
    checkRoundTrip(makeEntry("2026-01-01", "a-b_c-D9", "Deep work", 3600, 1.25, -5), "fractional-speed-negative-pitch");
    checkRoundTrip(makeEntry("2026-12-31", "zzz", "Pomodoro 12", 2700, 0.75, 7), "slow-speed");
    checkRoundTrip(makeEntry("", "", "", 60, 1.0, 0), "empty-text");
    checkRoundTrip(makeEntry("2026-10-09", "id", std::string(4000, 'x'), 300, 1.5, 2), "long-label");
    for (double speed : {0.5, 0.75, 1.0, 1.25, 1.5, 2.0}) {
        for (int pitch : {-6, -1, 0, 1, 6}) {
            StudyEntry entry = baseEntry();
            entry.speed = speed;
            entry.pitch = pitch;
            checkRoundTrip(entry, "speed-" + formatDouble(speed) + "-pitch-" + std::to_string(pitch));
        }
    }
}

static void testManyEntries()
{
    fs::path file = root / "long-list" / "study.json";
    std::vector<StudyEntry> wanted;
    bool all_true = true;
    for (int index = 0; index < 200; ++index) {
        int day_number = index % 20 + 1;
        std::string day = "2026-11-" + std::string(day_number < 10 ? "0" : "") + std::to_string(day_number);
        StudyEntry entry = makeEntry(day, "video-" + std::to_string(index),
                                     "Pomodoro " + std::to_string(index), 60 + index,
                                     1.0 + 0.25 * (index % 5), index % 13 - 6);
        if (!tryAppend(entry, file, "long list entry " + std::to_string(index)))
            all_true = false;
        wanted.push_back(entry);
    }
    check(all_true, "every appendStudy() of a long list of " + std::to_string(wanted.size()) + " entries returns true");
    std::vector<StudyEntry> loaded = tryLoad(file, "long list");
    check(loaded.size() == wanted.size(),
          "a long list loads back with every entry, want " + std::to_string(wanted.size())
              + ", got " + std::to_string(loaded.size()));
    check(sameEntries(loaded, wanted), "a long list loads back in the order it was appended");

    int sum = 0;
    for (const StudyEntry& entry : wanted)
        sum += entry.seconds;
    check(totalSeconds(loaded) == sum, "totalSeconds() of a long list is " + std::to_string(sum)
                                           + ", got " + std::to_string(totalSeconds(loaded)));

    int day_sum = 0;
    for (const StudyEntry& entry : wanted) {
        if (entry.day == "2026-11-07")
            day_sum += entry.seconds;
    }
    check(secondsOnDay(loaded, "2026-11-07") == day_sum,
          "secondsOnDay() of a long list for 2026-11-07 is " + std::to_string(day_sum)
              + ", got " + std::to_string(secondsOnDay(loaded, "2026-11-07")));
}

static void testTotalSeconds()
{
    std::vector<StudyEntry> empty;
    check(totalSeconds(empty) == 0, "totalSeconds() of an empty vector is 0, got " + std::to_string(totalSeconds(empty)));

    std::vector<StudyEntry> one{makeEntry("2026-10-09", "a", "Pomodoro 1", 1500, 1.0, 0)};
    check(totalSeconds(one) == 1500, "totalSeconds() of one 1500 second entry is 1500, got " + std::to_string(totalSeconds(one)));

    std::vector<StudyEntry> several{
        makeEntry("2026-10-09", "a", "Pomodoro 1", 1500, 1.0, 0),
        makeEntry("2026-10-09", "b", "Pomodoro 2", 1500, 1.25, 2),
        makeEntry("2026-10-10", "c", "Pomodoro 3", 600, 1.5, -2),
        makeEntry("2026-10-11", "d", "Pomodoro 4", 45, 1.0, 0),
    };
    check(totalSeconds(several) == 3645, "totalSeconds() across four entries is 3645, got " + std::to_string(totalSeconds(several)));

    std::vector<StudyEntry> with_zero = several;
    with_zero.push_back(makeEntry("2026-10-12", "e", "Pomodoro 5", 0, 1.0, 0));
    check(totalSeconds(with_zero) == 3645, "a zero second entry adds nothing to totalSeconds(), got "
                                               + std::to_string(totalSeconds(with_zero)));
}

static void testSecondsOnDay()
{
    std::vector<StudyEntry> empty;
    check(secondsOnDay(empty, "2026-10-09") == 0,
          "secondsOnDay() of an empty vector is 0, got " + std::to_string(secondsOnDay(empty, "2026-10-09")));
    check(secondsOnDay(empty, "") == 0,
          "secondsOnDay() of an empty vector for an empty day is 0, got " + std::to_string(secondsOnDay(empty, "")));

    std::vector<StudyEntry> entries{
        makeEntry("2026-10-09", "a", "Pomodoro 1", 1500, 1.0, 0),
        makeEntry("2026-10-09", "b", "Pomodoro 2", 300, 1.25, 1),
        makeEntry("2026-10-10", "c", "Pomodoro 3", 900, 1.0, 0),
        makeEntry("2026-10-11", "d", "Pomodoro 4", 60, 2.0, -4),
        makeEntry("2026-10-11", "e", "Pomodoro 5", 120, 1.5, 3),
        makeEntry("2026-10-11", "f", "Pomodoro 6", 180, 1.0, 0),
    };
    check(secondsOnDay(entries, "2026-10-09") == 1800,
          "secondsOnDay() for 2026-10-09 is 1800, got " + std::to_string(secondsOnDay(entries, "2026-10-09")));
    check(secondsOnDay(entries, "2026-10-10") == 900,
          "secondsOnDay() for 2026-10-10 is 900, got " + std::to_string(secondsOnDay(entries, "2026-10-10")));
    check(secondsOnDay(entries, "2026-10-11") == 360,
          "secondsOnDay() for 2026-10-11 is 360, got " + std::to_string(secondsOnDay(entries, "2026-10-11")));
    check(secondsOnDay(entries, "2026-10-12") == 0,
          "secondsOnDay() for a day with no entries is 0, got " + std::to_string(secondsOnDay(entries, "2026-10-12")));
    check(secondsOnDay(entries, "") == 0,
          "secondsOnDay() for an empty day is 0 when no entry has an empty day, got " + std::to_string(secondsOnDay(entries, "")));
    check(secondsOnDay(entries, "2026-10-9") == 0,
          "secondsOnDay() does not match 2026-10-9 against 2026-10-09, got " + std::to_string(secondsOnDay(entries, "2026-10-9")));
    check(secondsOnDay(entries, " 2026-10-09") == 0,
          "secondsOnDay() does not trim a leading space, got " + std::to_string(secondsOnDay(entries, " 2026-10-09")));
    check(secondsOnDay(entries, "2026-10-09 ") == 0,
          "secondsOnDay() does not trim a trailing space, got " + std::to_string(secondsOnDay(entries, "2026-10-09 ")));
    check(secondsOnDay(entries, "2026-10-09T00:00") == 0,
          "secondsOnDay() does not match a longer day string, got " + std::to_string(secondsOnDay(entries, "2026-10-09T00:00")));

    std::vector<StudyEntry> cased{
        makeEntry("MONDAY", "a", "Pomodoro 1", 100, 1.0, 0),
        makeEntry("monday", "b", "Pomodoro 2", 20, 1.0, 0),
        makeEntry("Monday", "c", "Pomodoro 3", 3, 1.0, 0),
    };
    check(secondsOnDay(cased, "MONDAY") == 100, "secondsOnDay() is case-sensitive for MONDAY, got " + std::to_string(secondsOnDay(cased, "MONDAY")));
    check(secondsOnDay(cased, "monday") == 20, "secondsOnDay() is case-sensitive for monday, got " + std::to_string(secondsOnDay(cased, "monday")));
    check(secondsOnDay(cased, "Monday") == 3, "secondsOnDay() is case-sensitive for Monday, got " + std::to_string(secondsOnDay(cased, "Monday")));

    std::vector<StudyEntry> blank{
        makeEntry("", "a", "Pomodoro 1", 70, 1.0, 0),
        makeEntry("2026-10-09", "b", "Pomodoro 2", 30, 1.0, 0),
        makeEntry("", "c", "Pomodoro 3", 5, 1.0, 0),
    };
    check(secondsOnDay(blank, "") == 75,
          "secondsOnDay() for an empty day sums the entries that genuinely have an empty day, got "
              + std::to_string(secondsOnDay(blank, "")));

    std::vector<StudyEntry> utf8{
        makeEntry("\xE6\x97\xA5\xE6\x9C\xAC", "a", "Pomodoro 1", 240, 1.0, 0),
        makeEntry("2026-10-09", "b", "Pomodoro 2", 60, 1.0, 0),
    };
    check(secondsOnDay(utf8, "\xE6\x97\xA5\xE6\x9C\xAC") == 240,
          "secondsOnDay() matches a non-ASCII day, got " + std::to_string(secondsOnDay(utf8, "\xE6\x97\xA5\xE6\x9C\xAC")));
}

static void testTotalsAfterLoad()
{
    fs::path file = root / "totals" / "study.json";
    std::vector<StudyEntry> wanted{
        makeEntry("2026-10-09", "a", "Pomodoro 1", 1500, 1.0, 0),
        makeEntry("2026-10-09", "b", "Pomodoro 2", 1500, 1.25, -2),
        makeEntry("2026-10-10", "c", "Pomodoro 1", 600, 1.5, 3),
        makeEntry("2026-10-11", "d", "Pomodoro 1", 1200, 0.75, 0),
        makeEntry("2026-10-11", "e", "Pomodoro 2", 1200, 2.0, 5),
    };
    for (const StudyEntry& entry : wanted)
        check(tryAppend(entry, file, "totals setup"), "appendStudy() for the totals setup returns true");

    std::vector<StudyEntry> loaded = tryLoad(file, "totals");
    check(sameEntries(loaded, wanted), "the totals file loads back every entry, got " + describeAll(loaded));
    check(totalSeconds(loaded) == 6000, "totalSeconds() of the loaded entries is 6000, got " + std::to_string(totalSeconds(loaded)));
    check(secondsOnDay(loaded, "2026-10-09") == 3000,
          "secondsOnDay() of the loaded entries for 2026-10-09 is 3000, got " + std::to_string(secondsOnDay(loaded, "2026-10-09")));
    check(secondsOnDay(loaded, "2026-10-10") == 600,
          "secondsOnDay() of the loaded entries for 2026-10-10 is 600, got " + std::to_string(secondsOnDay(loaded, "2026-10-10")));
    check(secondsOnDay(loaded, "2026-10-11") == 2400,
          "secondsOnDay() of the loaded entries for 2026-10-11 is 2400, got " + std::to_string(secondsOnDay(loaded, "2026-10-11")));
    check(secondsOnDay(loaded, "2026-10-08") == 0,
          "secondsOnDay() of the loaded entries for an unused day is 0, got " + std::to_string(secondsOnDay(loaded, "2026-10-08")));
}

static void testLoadTwice()
{
    fs::path file = root / "load-twice" / "study.json";
    StudyEntry entry = baseEntry();
    check(tryAppend(entry, file, "load twice setup"), "appendStudy() for the load twice setup returns true");
    std::vector<StudyEntry> first = tryLoad(file, "load twice first");
    std::vector<StudyEntry> second = tryLoad(file, "load twice second");
    check(sameEntries(first, std::vector<StudyEntry>{entry}), "the first load gives the appended entry, got " + describeAll(first));
    check(sameEntries(second, std::vector<StudyEntry>{entry}), "the second load gives the same entry, got " + describeAll(second));

    fs::path hand = root / "load-twice" / "hand.json";
    writeFile(hand, "[{\"day\": \"2026-10-09\", \"seconds\": 2.5, \"speed\": \"fast\"}, 7, " "{\"video_id\": \"x\", \"seconds\": 90, \"pitch\": 4}]");
    std::vector<StudyEntry> expected{
        makeEntry("2026-10-09", "", "", 0, 1.0, 0),
        makeEntry("", "x", "", 90, 1.0, 4),
    };
    std::vector<StudyEntry> hand_first = tryLoad(hand, "hand-edited first");
    std::vector<StudyEntry> hand_second = tryLoad(hand, "hand-edited second");
    check(sameEntries(hand_first, expected), "a hand-edited file loads with defaults filled in, got " + describeAll(hand_first));
    check(sameEntries(hand_second, expected), "loading the hand-edited file again gives the same entries, got " + describeAll(hand_second));
}

int main()
{
    root = fs::temp_directory_path() / ("youtonomous-study-log-test-" + std::to_string(GetCurrentProcessId()));
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root);

    testAppendToMissingFile();
    testAppendManyInARow();
    testNonPositiveSeconds();
    testAppendFailures();
    testAppendOverCorrupt();
    testLoadMissingFile();
    testLoadInvalidFiles();
    testLoadMixedContent();
    testMissingFields();
    testWrongTypes();
    testWholeNumbers();
    testUtf8Text();
    testExtraKeys();
    testRoundTrips();
    testManyEntries();
    testTotalSeconds();
    testSecondsOnDay();
    testTotalsAfterLoad();
    testLoadTwice();

    fs::remove_all(root, error);
    return report();
}
