#include "library_panel.h"

#include <algorithm>

#include <commctrl.h>

#include "text.h"
#include "timecode.h"

using namespace ui;

namespace {

const int kLengthColumn = 64;
const int kWatchedColumn = 64;
const SortBy kColumnSort[] = {SortBy::title, SortBy::length, SortBy::watched};

void addColumn(HWND list, int index, const wchar_t* title, int width, int format)
{
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    column.fmt = format;
    column.cx = width;
    column.pszText = const_cast<wchar_t*>(title);
    SendMessageW(list, LVM_INSERTCOLUMNW, index, reinterpret_cast<LPARAM>(&column));
}

void setCell(HWND list, int row, int column, const std::wstring& text)
{
    LVITEMW item{};
    item.iSubItem = column;
    item.pszText = const_cast<wchar_t*>(text.c_str());
    SendMessageW(list, LVM_SETITEMTEXTW, row, reinterpret_cast<LPARAM>(&item));
}

}

void LibraryPanel::create(const Context& context)
{
    const DWORD list_style = LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;

    url_edit_ = addControl(context, L"EDIT", L"", ES_AUTOHSCROLL, kUrl, WS_EX_CLIENTEDGE);
    download_button_ = addControl(context, L"BUTTON", L"Download", BS_PUSHBUTTON, kDownload);
    progress_ = addControl(context, PROGRESS_CLASSW, L"", 0, 0);
    status_ = addControl(context, L"STATIC", L"Paste a YouTube link above.", SS_LEFT, 0);
    label_ = addControl(context, L"STATIC", L"Library", SS_LEFT, 0);
    search_edit_ = addControl(context, L"EDIT", L"", ES_AUTOHSCROLL, kSearch, WS_EX_CLIENTEDGE);
    list_ = addControl(context, WC_LISTVIEWW, L"", list_style, kLibrary, WS_EX_CLIENTEDGE);
    open_button_ = addControl(context, L"BUTTON", L"Open file...", BS_PUSHBUTTON, kOpen);
    remove_button_ = addControl(context, L"BUTTON", L"Remove", BS_PUSHBUTTON, kRemove);

    SendMessageW(progress_, PBM_SETRANGE32, 0, 100);
    SendMessageW(search_edit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Search titles"));

    DWORD extended = LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_DOUBLEBUFFER;
    SendMessageW(list_, LVM_SETEXTENDEDLISTVIEWSTYLE, extended, extended);
    addColumn(list_, 0, L"Title", 100, LVCFMT_LEFT);
    addColumn(list_, 1, L"Length", kLengthColumn, LVCFMT_RIGHT);
    addColumn(list_, 2, L"Watched", kWatchedColumn, LVCFMT_RIGHT);
}

void LibraryPanel::layout(int height)
{
    int bottom = height - kPad - kRow;
    int half = (kSide - kGap) / 2;

    int y = kPad;
    MoveWindow(url_edit_, kPad, y, kSide - kSmallButton - kGap, kRow, TRUE);
    MoveWindow(download_button_, kPad + kSide - kSmallButton, y, kSmallButton, kRow, TRUE);
    y += kRow + kGap;
    MoveWindow(progress_, kPad, y, kSide, 14, TRUE);
    y += 14 + kGap;
    MoveWindow(status_, kPad, y, kSide, kLabel * 2, TRUE);
    y += kLabel * 2 + kPad;
    MoveWindow(label_, kPad, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(search_edit_, kPad, y, kSide, kRow, TRUE);
    y += kRow + kGap;
    MoveWindow(list_, kPad, y, kSide, std::max(0, bottom - kGap - y), TRUE);
    MoveWindow(open_button_, kPad, bottom, half, kRow, TRUE);
    MoveWindow(remove_button_, kPad + half + kGap, bottom, half, kRow, TRUE);

    RECT inside;
    GetClientRect(list_, &inside);
    int title_width = std::max(60, static_cast<int>(inside.right) - kLengthColumn - kWatchedColumn);
    SendMessageW(list_, LVM_SETCOLUMNWIDTH, 0, title_width);
}

void LibraryPanel::setVisible(bool visible)
{
    ui::setVisible({url_edit_, download_button_, progress_, status_, label_, search_edit_, list_, open_button_, remove_button_},
                   visible);
}

std::string LibraryPanel::url() const
{
    return trim(narrow(textOf(url_edit_)));
}

void LibraryPanel::clearUrl()
{
    SetWindowTextW(url_edit_, L"");
}

std::string LibraryPanel::search() const
{
    return narrow(textOf(search_edit_));
}

void LibraryPanel::sortByColumn(int column)
{
    if (column < 0 || column > 2)
        return;

    SortBy chosen = kColumnSort[column];
    if (chosen != sort_by_) {
        sort_by_ = chosen;
        descending_ = chosen != SortBy::title;
    } else if (descending_ == (chosen != SortBy::title)) {
        descending_ = !descending_;
    } else {
        sort_by_ = SortBy::added;
        descending_ = false;
    }
}

void LibraryPanel::showProgress(int percent)
{
    SendMessageW(progress_, PBM_SETPOS, percent, 0);
}

void LibraryPanel::showStatus(const std::wstring& text)
{
    setText(status_, text);
}

void LibraryPanel::setDownloading(bool downloading)
{
    EnableWindow(download_button_, downloading ? FALSE : TRUE);
}

void LibraryPanel::showVideos(const std::vector<Video>& videos)
{
    rows_ = libraryRows(videos, search(), sort_by_, descending_);

    SendMessageW(list_, WM_SETREDRAW, FALSE, 0);
    SendMessageW(list_, LVM_DELETEALLITEMS, 0, 0);
    for (size_t row = 0; row < rows_.size(); ++row) {
        const Video& video = videos[rows_[row]];
        std::wstring title = widen(video.title);

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(row);
        item.pszText = const_cast<wchar_t*>(title.c_str());
        SendMessageW(list_, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));

        setCell(list_, static_cast<int>(row), 1, video.duration > 0 ? widen(timecode::format(video.duration)) : L"");
        setCell(list_, static_cast<int>(row), 2, video.duration > 0 ? std::to_wstring(watchedPercent(video)) + L"%" : L"");
    }
    SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
    showSortArrow();
}

void LibraryPanel::showSortArrow()
{
    HWND header = reinterpret_cast<HWND>(SendMessageW(list_, LVM_GETHEADER, 0, 0));
    for (int column = 0; column < 3; ++column) {
        HDITEMW item{};
        item.mask = HDI_FORMAT;
        SendMessageW(header, HDM_GETITEMW, column, reinterpret_cast<LPARAM>(&item));
        item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (kColumnSort[column] == sort_by_)
            item.fmt |= descending_ ? HDF_SORTDOWN : HDF_SORTUP;
        SendMessageW(header, HDM_SETITEMW, column, reinterpret_cast<LPARAM>(&item));
    }
}

void LibraryPanel::select(int index)
{
    LVITEMW clear{};
    clear.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(list_, LVM_SETITEMSTATE, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(&clear));

    auto found = std::find(rows_.begin(), rows_.end(), index);
    if (found == rows_.end())
        return;

    int row = static_cast<int>(found - rows_.begin());
    LVITEMW item{};
    item.state = LVIS_SELECTED | LVIS_FOCUSED;
    item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(list_, LVM_SETITEMSTATE, row, reinterpret_cast<LPARAM>(&item));
    SendMessageW(list_, LVM_ENSUREVISIBLE, row, FALSE);
}

int LibraryPanel::selected(size_t count) const
{
    int row = static_cast<int>(SendMessageW(list_, LVM_GETNEXTITEM, static_cast<WPARAM>(-1), LVNI_SELECTED));
    if (row < 0 || static_cast<size_t>(row) >= rows_.size())
        return -1;
    int index = rows_[row];
    return static_cast<size_t>(index) < count ? index : -1;
}
