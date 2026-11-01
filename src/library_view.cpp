#include "library_view.h"

#include <algorithm>

static std::string folded(const std::string& text)
{
    std::string result = text;
    for (char& c : result) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return result;
}

static bool byteLess(const std::string& a, const std::string& b)
{
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y) {
        return static_cast<unsigned char>(x) < static_cast<unsigned char>(y);
    });
}

bool matchesSearch(const Video& video, const std::string& search)
{
    std::string title = folded(video.title);
    std::string words = folded(search);

    size_t at = 0;
    while (at < words.size()) {
        size_t begin = words.find_first_not_of(" \t", at);
        if (begin == std::string::npos)
            break;
        size_t end = words.find_first_of(" \t", begin);
        if (end == std::string::npos)
            end = words.size();
        if (title.find(words.substr(begin, end - begin)) == std::string::npos)
            return false;
        at = end;
    }
    return true;
}

static int studiedFor(const std::map<std::string, int>& studied, const std::string& id)
{
    auto found = studied.find(id);
    return found == studied.end() ? 0 : found->second;
}

std::vector<int> libraryRows(const std::vector<Video>& videos, const std::map<std::string, int>& studied, const std::string& search,
                             SortBy sort_by, bool descending)
{
    std::vector<int> rows;
    for (size_t i = 0; i < videos.size(); ++i) {
        if (matchesSearch(videos[i], search))
            rows.push_back(static_cast<int>(i));
    }

    if (sort_by == SortBy::added) {
        if (descending)
            std::reverse(rows.begin(), rows.end());
        return rows;
    }

    auto less = [&](int a, int b) {
        const Video& first = videos[a];
        const Video& second = videos[b];
        switch (sort_by) {
        case SortBy::title: return byteLess(folded(first.title), folded(second.title));
        case SortBy::length: return first.duration < second.duration;
        default: return studiedFor(studied, first.id) < studiedFor(studied, second.id);
        }
    };
    std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) { return descending ? less(b, a) : less(a, b); });
    return rows;
}
