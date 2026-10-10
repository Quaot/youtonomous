#include "study_log.h"

#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

using nlohmann::json;

static std::string readString(const json& item, const char* key)
{
    if (!item.contains(key) || !item[key].is_string())
        return "";
    return item[key].get<std::string>();
}

static int readWhole(const json& item, const char* key)
{
    if (!item.contains(key) || !item[key].is_number_integer())
        return 0;
    return item[key].get<int>();
}

static StudyEntry fromJson(const json& item)
{
    StudyEntry entry;
    entry.day = readString(item, "day");
    entry.video_id = readString(item, "video_id");
    entry.label = readString(item, "label");
    entry.seconds = readWhole(item, "seconds");
    entry.pitch = readWhole(item, "pitch");
    if (item.contains("speed") && item["speed"].is_number())
        entry.speed = item["speed"].get<double>();
    return entry;
}

std::vector<StudyEntry> loadStudy(const std::filesystem::path& file)
{
    std::vector<StudyEntry> entries;
    try {
        std::ifstream in(file, std::ios::binary);
        if (!in)
            return entries;

        json items = json::parse(in, nullptr, false);
        if (items.is_discarded() || !items.is_array())
            return entries;

        for (const json& item : items) {
            if (item.is_object())
                entries.push_back(fromJson(item));
        }
    } catch (...) {
        entries.clear();
    }
    return entries;
}

bool appendStudy(const StudyEntry& entry, const std::filesystem::path& file)
{
    if (entry.seconds <= 0)
        return true;

    try {
        std::vector<StudyEntry> entries = loadStudy(file);
        entries.push_back(entry);

        json items = json::array();
        for (const StudyEntry& one : entries) {
            items.push_back({
                {"day", one.day},
                {"video_id", one.video_id},
                {"label", one.label},
                {"seconds", one.seconds},
                {"speed", one.speed},
                {"pitch", one.pitch},
            });
        }

        std::error_code error;
        std::filesystem::create_directories(file.parent_path(), error);

        std::filesystem::path temp = file;
        temp += ".tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out << items.dump(2);
            if (!out) {
                std::filesystem::remove(temp, error);
                return false;
            }
        }

        std::filesystem::rename(temp, file, error);
        if (error) {
            std::filesystem::remove(temp, error);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

int totalSeconds(const std::vector<StudyEntry>& entries)
{
    int total = 0;
    for (const StudyEntry& entry : entries)
        total += entry.seconds;
    return total;
}

int secondsOnDay(const std::vector<StudyEntry>& entries, const std::string& day)
{
    int total = 0;
    for (const StudyEntry& entry : entries) {
        if (entry.day == day)
            total += entry.seconds;
    }
    return total;
}
