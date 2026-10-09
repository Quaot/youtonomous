#include "settings.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

using nlohmann::json; // settings, for setting things... :)

static int readInt(const json& item, const char* key, int fallback)
{
    if (!item.contains(key) || !item[key].is_number_integer())
        return fallback;
    long long value = item[key].get<long long>();
    return static_cast<int>(std::clamp<long long>(value, INT_MIN, INT_MAX));
}

static std::string readString(const json& item, const char* key, const std::string& fallback)
{
    if (!item.contains(key) || !item[key].is_string())
        return fallback;
    return item[key].get<std::string>();
}

static Settings fromJson(const json& item)
{
    Settings defaults;
    Settings settings;

    settings.backend = readString(item, "backend", defaults.backend);
    if (settings.backend != "vlc" && settings.backend != "mpv")
        settings.backend = defaults.backend;

    settings.videos_folder = readString(item, "videos_folder", defaults.videos_folder);
    settings.window_x = readInt(item, "window_x", defaults.window_x);
    settings.window_y = readInt(item, "window_y", defaults.window_y);

    settings.window_width = readInt(item, "window_width", defaults.window_width);
    if (settings.window_width < 320)
        settings.window_width = defaults.window_width;
    settings.window_height = readInt(item, "window_height", defaults.window_height);
    if (settings.window_height < 240)
        settings.window_height = defaults.window_height;

    if (item.contains("maximized") && item["maximized"].is_boolean())
        settings.maximized = item["maximized"].get<bool>();

    settings.volume = std::clamp(readInt(item, "volume", defaults.volume), 0, 100);
    settings.pitch = std::clamp(readInt(item, "pitch", defaults.pitch), -6, 6);

    if (item.contains("keep_pitch") && item["keep_pitch"].is_boolean())
        settings.keep_pitch = item["keep_pitch"].get<bool>();

    if (item.contains("speed") && item["speed"].is_number()) {
        double speed = item["speed"].get<double>();
        settings.speed = std::isfinite(speed) ? std::clamp(speed, 0.5, 2.0) : defaults.speed;
    }
    return settings;
}

Settings loadSettings(const std::filesystem::path& file)
{
    try {
        std::ifstream in(file, std::ios::binary);
        if (!in)
            return Settings();

        json item = json::parse(in, nullptr, false);
        if (item.is_discarded() || !item.is_object())
            return Settings();
        return fromJson(item);
    } catch (...) {
        return Settings();
    }
}

bool saveSettings(const Settings& settings, const std::filesystem::path& file)
{
    try {
        json item = {
            {"backend", settings.backend},
            {"videos_folder", settings.videos_folder},
            {"window_x", settings.window_x},
            {"window_y", settings.window_y},
            {"window_width", settings.window_width},
            {"window_height", settings.window_height},
            {"maximized", settings.maximized},
            {"volume", settings.volume},
            {"speed", settings.speed},
            {"pitch", settings.pitch},
            {"keep_pitch", settings.keep_pitch}};

        std::error_code error;
        std::filesystem::create_directories(file.parent_path(), error);

        std::filesystem::path temp = file;
        temp += ".tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out << item.dump(2);
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
