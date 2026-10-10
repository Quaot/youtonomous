#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct StudyEntry {
    std::string day;
    std::string video_id;
    std::string label;
    int seconds = 0;
    double speed = 1.0;
    int pitch = 0;
};

bool appendStudy(const StudyEntry& entry, const std::filesystem::path& file);
std::vector<StudyEntry> loadStudy(const std::filesystem::path& file);
int totalSeconds(const std::vector<StudyEntry>& entries);
int secondsOnDay(const std::vector<StudyEntry>& entries, const std::string& day);
