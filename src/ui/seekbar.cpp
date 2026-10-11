#include "seekbar.h"

#include <algorithm>
#include <string>

#include <windowsx.h>

#include "timecode.h"

namespace seekbar {

namespace {

const wchar_t kClassName[] = L"YoutonomousSeekBar";

const COLORREF kTrack = RGB(200, 200, 200);
const COLORREF kPlayed = RGB(220, 40, 40);
const COLORREF kChapter = RGB(60, 110, 200);
const COLORREF kMark = RGB(240, 150, 0);
const COLORREF kStart = RGB(40, 170, 80);

const int kHoverReach = 5;

struct State {
    int length = 0;
    int position = 0;
    int start = 0;
    std::vector<Bookmark> marks;
    bool dragging = false;
    bool hovering = false;
    int hover_x = 0;
    std::string hover_text;
};

State* stateOf(HWND bar)
{
    return reinterpret_cast<State*>(GetWindowLongPtrW(bar, GWLP_USERDATA));
}

int widthOf(HWND bar)
{
    RECT client;
    GetClientRect(bar, &client);
    return client.right;
}

int xFor(const State& state, int seconds, int width)
{
    if (state.length <= 0)
        return 0;
    return static_cast<int>(static_cast<long long>(seconds) * width / state.length);
}

int secondsFor(const State& state, int x, int width)
{
    if (width <= 0)
        return 0;
    x = std::clamp(x, 0, width);
    return static_cast<int>(static_cast<long long>(x) * state.length / width);
}

const Bookmark* markNear(const State& state, int x, int width)
{
    const Bookmark* found = nullptr;
    int best = kHoverReach + 1;
    for (const Bookmark& mark : state.marks) {
        if (mark.label.empty() || mark.time < 0 || mark.time > state.length)
            continue;
        int distance = std::abs(xFor(state, mark.time, width) - x);
        if (distance <= kHoverReach && distance < best) {
            best = distance;
            found = &mark;
        }
    }
    return found;
}

std::string hoverTextFor(const State& state, int x, int width)
{
    const Bookmark* mark = markNear(state, x, width);
    if (mark)
        return timecode::format(mark->time) + "  " + mark->label;
    return timecode::format(secondsFor(state, x, width));
}

std::wstring toWide(const std::string& text)
{
    if (text.empty())
        return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

void fill(HDC dc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void drawTick(HDC dc, int x, int height, COLORREF color)
{
    RECT rect{x - 1, 2, x + 2, height - 2};
    fill(dc, rect, color);
}

void drawTooltip(HDC dc, const State& state, int width)
{
    std::wstring text = toWide(state.hover_text);
    if (text.empty())
        return;

    HGDIOBJ old_font = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));

    RECT measure{0, 0, 0, 0};
    DrawTextW(dc, text.c_str(), -1, &measure, DT_CALCRECT | DT_SINGLELINE);

    const int pad = 4;
    int box_width = measure.right - measure.left + pad * 2;
    int box_height = measure.bottom - measure.top + pad * 2;
    int left = std::clamp(state.hover_x - box_width / 2, 0, std::max(0, width - box_width));

    RECT box{left, 0, left + box_width, box_height};
    fill(dc, box, GetSysColor(COLOR_INFOBK));
    FrameRect(dc, &box, reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_INFOTEXT));
    RECT text_rect{left + pad, pad, left + box_width - pad, box_height - pad};
    DrawTextW(dc, text.c_str(), -1, &text_rect, DT_SINGLELINE | DT_LEFT | DT_TOP);

    SelectObject(dc, old_font);
}

void draw(HDC dc, const State& state, int width, int height)
{
    RECT all{0, 0, width, height};
    fill(dc, all, GetSysColor(COLOR_BTNFACE));

    RECT track{0, height / 2 - 3, width, height / 2 + 3};
    fill(dc, track, kTrack);

    track.right = xFor(state, state.position, width);
    fill(dc, track, kPlayed);

    if (state.length <= 0)
        return;

    for (const Bookmark& mark : state.marks)
        drawTick(dc, xFor(state, mark.time, width), height, mark.chapter ? kChapter : kMark);

    if (state.start > 0)
        drawTick(dc, xFor(state, state.start, width), height, kStart);

    if (state.hovering)
        drawTooltip(dc, state, width);
}

void paint(HWND bar, const State& state)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(bar, &ps);

    RECT client;
    GetClientRect(bar, &client);

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
    HGDIOBJ old = SelectObject(memory, bitmap);

    draw(memory, state, client.right, client.bottom);
    BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);

    SelectObject(memory, old);
    DeleteObject(bitmap);
    DeleteDC(memory);
    EndPaint(bar, &ps);
}

void dragTo(HWND bar, State& state, int x)
{
    state.position = secondsFor(state, x, widthOf(bar));
    InvalidateRect(bar, nullptr, FALSE);
}

void clearHover(HWND bar, State& state)
{
    if (!state.hovering)
        return;
    state.hovering = false;
    state.hover_text.clear();
    InvalidateRect(bar, nullptr, FALSE);
}

void trackLeave(HWND bar)
{
    TRACKMOUSEEVENT track{};
    track.cbSize = sizeof track;
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = bar;
    TrackMouseEvent(&track);
}

void hoverTo(HWND bar, State& state, int x)
{
    int width = widthOf(bar);
    if (state.length <= 0 || width <= 0) {
        clearHover(bar, state);
        return;
    }
    x = std::clamp(x, 0, width);
    state.hovering = true;
    state.hover_x = x;
    state.hover_text = hoverTextFor(state, x, width);
    trackLeave(bar);
    InvalidateRect(bar, nullptr, FALSE);
}

LRESULT CALLBACK proc(HWND bar, UINT msg, WPARAM w_param, LPARAM l_param)
{
    State* state = stateOf(bar);

    switch (msg) {
    case WM_NCCREATE:
        SetWindowLongPtrW(bar, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(new State));
        break;
    case WM_NCDESTROY:
        delete state;
        SetWindowLongPtrW(bar, GWLP_USERDATA, 0);
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint(bar, *state);
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(GetParent(bar));
        if (state->length > 0 && widthOf(bar) > 0) {
            clearHover(bar, *state);
            state->dragging = true;
            SetCapture(bar);
            dragTo(bar, *state, GET_X_LPARAM(l_param));
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state->dragging)
            dragTo(bar, *state, GET_X_LPARAM(l_param));
        else
            hoverTo(bar, *state, GET_X_LPARAM(l_param));
        return 0;
    case WM_MOUSELEAVE:
        clearHover(bar, *state);
        return 0;
    case WM_LBUTTONUP:
        if (state->dragging) {
            state->dragging = false;
            ReleaseCapture();
            PostMessageW(GetParent(bar), WM_SEEKBAR_SEEK, state->position, 0);
        }
        return 0;
    case WM_CAPTURECHANGED:
        state->dragging = false;
        return 0;
    }
    return DefWindowProcW(bar, msg, w_param, l_param);
}

}

HWND create(HWND parent, HINSTANCE instance)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_HAND);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    return CreateWindowExW(0, kClassName, L"", WS_CHILD | WS_VISIBLE,
                           0, 0, 0, 0, parent, nullptr, instance, nullptr);
}

void setRange(HWND bar, int length)
{
    State* state = stateOf(bar);
    if (state->length == length)
        return;
    state->length = length;
    InvalidateRect(bar, nullptr, FALSE);
}

void setPosition(HWND bar, int position)
{
    State* state = stateOf(bar);
    if (state->dragging || state->position == position)
        return;
    state->position = position;
    InvalidateRect(bar, nullptr, FALSE);
}

void setMarks(HWND bar, const std::vector<Bookmark>& marks, int start)
{
    State* state = stateOf(bar);
    state->marks = marks;
    state->start = start;
    InvalidateRect(bar, nullptr, FALSE);
}

std::string hoverText(HWND bar)
{
    State* state = stateOf(bar);
    if (!state || !state->hovering)
        return std::string();
    return state->hover_text;
}

}
