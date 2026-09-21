#pragma once

#include <string>
#include <vector>

#include <windows.h>

#include "library.h"

const UINT WM_SEEKBAR_SEEK = WM_APP + 10;

namespace seekbar {

HWND create(HWND parent, HINSTANCE instance);
void setRange(HWND bar, int length);
void setPosition(HWND bar, int position);
void setMarks(HWND bar, const std::vector<Bookmark>& marks, int start);
std::string hoverText(HWND bar);

}
