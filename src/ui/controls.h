#pragma once

#include <initializer_list>
#include <string>

#include <windows.h>

namespace ui {

enum ControlId {
    kOpen = 100,
    kRemove,
    kUrl,
    kDownload,
    kLibrary,
    kPrevMark,
    kBack30,
    kBack10,
    kPlay,
    kForward10,
    kForward30,
    kNextMark,
    kStart,
    kSetStart,
    kGoStart,
    kMarks,
    kMarkLabel,
    kAddMark,
    kDeleteMark,
    kSpeed,
    kVolume,
    kVideo,
    kResume,
    kEditMark,
    kMarkNote,
    kPitch,
    kKeepPitch,
    kBreak,
    kSilence,
    kLoop,
};

const int kPad = 8;
const int kGap = 4;
const int kSide = 270;
const int kRow = 26;
const int kLabel = 20;
const int kButton = 56;
const int kSmallButton = 80;
const int kSeekHeight = 20;

struct Context {
    HWND parent = nullptr;
    HINSTANCE instance = nullptr;
    HFONT font = nullptr;
};

HWND addControl(const Context& context, const wchar_t* type, const wchar_t* text, DWORD style, int id, DWORD ex_style = 0);
void setVisible(std::initializer_list<HWND> windows, bool visible);

std::wstring textOf(HWND control);
void setText(HWND control, const std::wstring& text);
std::string trim(const std::string& text);
int selectedIndex(HWND list, size_t count);

}
