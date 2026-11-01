#pragma once

#include <string>
#include <vector>

#include <windows.h>

#include "controls.h"
#include "library.h"
#include "library_view.h"

class LibraryPanel {
public:
    void create(const ui::Context& context);
    void layout(int height);
    void setVisible(bool visible);

    std::string url() const;
    void clearUrl();
    bool isUrlBox(HWND window) const { return window == url_edit_; }
    bool isSearchBox(HWND window) const { return window == search_edit_; }
    bool isList(HWND window) const { return window == list_; }
    std::string search() const;
    void sortByColumn(int column);

    void showProgress(int percent);
    void showStatus(const std::wstring& text);
    void setDownloading(bool downloading);

    void showVideos(const std::vector<Video>& videos);
    void select(int index);
    int selected(size_t count) const;
    int firstShown() const { return rows_.empty() ? -1 : rows_.front(); }

private:
    HWND url_edit_ = nullptr;
    HWND download_button_ = nullptr;
    HWND progress_ = nullptr;
    HWND status_ = nullptr;
    HWND label_ = nullptr;
    HWND search_edit_ = nullptr;
    HWND list_ = nullptr;
    HWND open_button_ = nullptr;
    HWND remove_button_ = nullptr;

    SortBy sort_by_ = SortBy::added;
    bool descending_ = false;
    std::vector<int> rows_;

    void showSortArrow();
};
