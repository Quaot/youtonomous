#pragma once

#include <map>
#include <string>
#include <vector>

#include "library.h"

enum class SortBy { added, title, length, studied };

bool matchesSearch(const Video& video, const std::string& search);
std::vector<int> libraryRows(const std::vector<Video>& videos, const std::map<std::string, int>& studied, const std::string& search,
                             SortBy sort_by, bool descending);
