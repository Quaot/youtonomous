#pragma once

#include <string>
#include <vector>

#include "library.h"

enum class SortBy { added, title, length, watched };

int watchedPercent(const Video& video);
bool matchesSearch(const Video& video, const std::string& search);
std::vector<int> libraryRows(const std::vector<Video>& videos, const std::string& search, SortBy sort_by, bool descending);
