#include <climits>
#include <cmath>
#include <string>
#include <vector>

#include <windows.h>

#include "check.h"
#include "seekbar.h"
#include "timecode.h"

static HWND parent;
static std::vector<long long> sent_seeks;

static LRESULT CALLBACK parentProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
    if (msg == WM_SEEKBAR_SEEK) {
        sent_seeks.push_back(static_cast<long long>(w_param));
        return 0;
    }
    return DefWindowProcW(hwnd, msg, w_param, l_param);
}

static std::vector<long long> takeSeeks()
{
    std::vector<long long> seeks = sent_seeks;
    sent_seeks.clear();
    MSG msg;
    while (PeekMessageW(&msg, parent, WM_SEEKBAR_SEEK, WM_SEEKBAR_SEEK, PM_REMOVE))
        seeks.push_back(static_cast<long long>(msg.wParam));
    return seeks;
}

static std::string list(const std::vector<long long>& seeks)
{
    std::string result = "[";
    for (long long seek : seeks)
        result += (result.size() > 1 ? " " : "") + std::to_string(seek);
    return result + "]";
}

static int clientWidth(HWND bar)
{
    RECT rect;
    GetClientRect(bar, &rect);
    return rect.right - rect.left;
}

static int middleY(HWND bar)
{
    RECT rect;
    GetClientRect(bar, &rect);
    return (rect.bottom - rect.top) / 2;
}

static void press(HWND bar, UINT msg, WPARAM buttons, int x, int y)
{
    SendMessageW(bar, msg, buttons, MAKELPARAM(static_cast<WORD>(static_cast<short>(x)), static_cast<WORD>(static_cast<short>(y))));
}

static void click(HWND bar, int x)
{
    int y = middleY(bar);
    press(bar, WM_LBUTTONDOWN, MK_LBUTTON, x, y);
    press(bar, WM_LBUTTONUP, 0, x, y);
}

static void drag(HWND bar, int from, int to)
{
    int y = middleY(bar);
    press(bar, WM_LBUTTONDOWN, MK_LBUTTON, from, y);
    press(bar, WM_MOUSEMOVE, MK_LBUTTON, (from + to) / 2, y);
    press(bar, WM_MOUSEMOVE, MK_LBUTTON, to, y);
    press(bar, WM_LBUTTONUP, 0, to, y);
}

static bool allWithin(const std::vector<long long>& seeks, long long length)
{
    for (long long seek : seeks)
        if (seek < 0 || seek > length)
            return false;
    return true;
}

static bool closeTo(const std::vector<long long>& seeks, int length, int x, int width)
{
    if (seeks.empty() || width <= 0)
        return false;
    double expected = static_cast<double>(x) * length / width;
    double tolerance = 1.0 + static_cast<double>(length) / width;
    return std::fabs(static_cast<double>(seeks.back()) - expected) <= tolerance;
}

static std::string describe(const std::string& action, int length, int width)
{
    return action + " on bar of length " + std::to_string(length) + " and client width " + std::to_string(width);
}

static void expectClick(HWND bar, int length, int x)
{
    int width = clientWidth(bar);
    click(bar, x);
    std::vector<long long> seeks = takeSeeks();
    std::string name = describe("click at x=" + std::to_string(x), length, width);
    check(!seeks.empty(), name + " posts WM_SEEKBAR_SEEK");
    check(allWithin(seeks, length), name + " seeks within 0.." + std::to_string(length) + ", got " + list(seeks));
    check(closeTo(seeks, length, x, width), name + " seeks to about " + std::to_string(static_cast<double>(x) * length / width) + ", got " + list(seeks));
}

static void expectDrag(HWND bar, int length, int from, int to)
{
    int width = clientWidth(bar);
    drag(bar, from, to);
    std::vector<long long> seeks = takeSeeks();
    std::string name = describe("drag from x=" + std::to_string(from) + " to x=" + std::to_string(to), length, width);
    check(!seeks.empty(), name + " posts WM_SEEKBAR_SEEK");
    check(allWithin(seeks, length), name + " seeks within 0.." + std::to_string(length) + ", got " + list(seeks));
    check(closeTo(seeks, length, to, width), name + " ends at about " + std::to_string(static_cast<double>(to) * length / width) + ", got " + list(seeks));
}

static void expectInRange(HWND bar, int length, const std::string& action)
{
    std::vector<long long> seeks = takeSeeks();
    check(allWithin(seeks, length), describe(action, length, clientWidth(bar)) + " never seeks outside 0.." + std::to_string(length) + ", got " + list(seeks));
}

static void expectNothing(HWND bar, int length, const std::string& action)
{
    std::vector<long long> seeks = takeSeeks();
    check(seeks.empty(), describe(action, length, clientWidth(bar)) + " posts nothing, got " + list(seeks));
}

static void paint(HWND bar)
{
    InvalidateRect(bar, nullptr, TRUE);
    SendMessageW(bar, WM_PAINT, 0, 0);
}

static void testClicks(HWND bar)
{
    MoveWindow(bar, 0, 0, 600, 24, TRUE);
    int width = clientWidth(bar);
    check(width > 0, "bar has a positive client width after MoveWindow, got " + std::to_string(width));

    click(bar, width / 2);
    expectNothing(bar, 0, "click before setRange");

    seekbar::setRange(bar, 300);
    expectClick(bar, 300, 0);
    expectClick(bar, 300, 1);
    expectClick(bar, 300, width / 4);
    expectClick(bar, 300, width / 2);
    expectClick(bar, 300, 3 * width / 4);
    expectClick(bar, 300, width - 1);

    click(bar, width / 2);
    click(bar, width / 4);
    std::vector<long long> seeks = takeSeeks();
    check(closeTo(seeks, 300, width / 4, width), "second of two clicks decides the last seek, got " + list(seeks));

    for (int x = 0; x < width; x += width / 10)
        press(bar, WM_MOUSEMOVE, 0, x, middleY(bar));
    expectNothing(bar, 300, "moving the mouse without a button");

    click(bar, -30);
    expectInRange(bar, 300, "click at x=-30");
    click(bar, width + 50);
    expectInRange(bar, 300, "click past the right edge");
    click(bar, -32000);
    expectInRange(bar, 300, "click at x=-32000");
    click(bar, 32000);
    expectInRange(bar, 300, "click at x=32000");
}

static void testDrags(HWND bar)
{
    int width = clientWidth(bar);
    seekbar::setRange(bar, 300);
    expectDrag(bar, 300, width / 4, 3 * width / 4);
    expectDrag(bar, 300, 9 * width / 10, width / 10);
    expectDrag(bar, 300, width / 2, width / 2);
    expectDrag(bar, 300, 0, width - 1);

    drag(bar, width / 2, -100);
    expectInRange(bar, 300, "drag past the left edge");
    drag(bar, width / 2, width + 100);
    expectInRange(bar, 300, "drag past the right edge");
}

static void testMarksAndPosition(HWND bar)
{
    int width = clientWidth(bar);
    seekbar::setRange(bar, 300);

    std::vector<Bookmark> marks(5);
    marks[0].time = 0;
    marks[0].chapter = true;
    marks[1].time = 150;
    marks[1].label = "mine";
    marks[2].time = 300;
    marks[2].chapter = true;
    marks[3].time = -10;
    marks[4].time = 100000;
    seekbar::setMarks(bar, marks, 50);
    seekbar::setPosition(bar, 120);
    paint(bar);
    expectClick(bar, 300, width / 2);

    seekbar::setMarks(bar, marks, -5);
    seekbar::setMarks(bar, marks, 100000);
    seekbar::setPosition(bar, -5);
    paint(bar);
    seekbar::setPosition(bar, 100000);
    paint(bar);
    seekbar::setMarks(bar, {}, 0);
    seekbar::setPosition(bar, 0);
    paint(bar);
    expectClick(bar, 300, width / 4);

    seekbar::setRange(bar, 0);
    seekbar::setMarks(bar, marks, 50);
    seekbar::setPosition(bar, 20);
    paint(bar);
    check(true, "painting with length 0 and marks does not crash");
    takeSeeks();
}

static void testLengths(HWND bar)
{
    int width = clientWidth(bar);
    for (int length : {1, 59, 60, 3600, 36000, 86400}) {
        seekbar::setRange(bar, length);
        expectClick(bar, length, width / 2);
        expectClick(bar, length, width - 1);
    }
    for (int length : {10000000, INT_MAX}) {
        seekbar::setRange(bar, length);
        expectClick(bar, length, width / 2);
        expectClick(bar, length, width - 1);
        paint(bar);
    }
}

static void testZeroLength(HWND bar)
{
    int width = clientWidth(bar);
    seekbar::setRange(bar, 300);
    seekbar::setRange(bar, 0);
    click(bar, width / 2);
    expectNothing(bar, 0, "click after setRange(0)");
    click(bar, 0);
    click(bar, width - 1);
    expectNothing(bar, 0, "clicks at the edges after setRange(0)");
    drag(bar, 0, width - 1);
    expectNothing(bar, 0, "drag after setRange(0)");

    seekbar::setRange(bar, 300);
    expectClick(bar, 300, width / 2);
}

static void testResize(HWND bar)
{
    seekbar::setRange(bar, 300);
    MoveWindow(bar, 0, 0, 300, 24, TRUE);
    int width = clientWidth(bar);
    expectClick(bar, 300, width / 2);
    expectClick(bar, 300, width / 4);

    MoveWindow(bar, 0, 0, 1200, 24, TRUE);
    width = clientWidth(bar);
    expectClick(bar, 300, width / 2);
    expectClick(bar, 300, 3 * width / 4);

    MoveWindow(bar, 0, 0, 0, 24, TRUE);
    paint(bar);
    click(bar, 0);
    expectInRange(bar, 300, "click on a zero-width bar");
    drag(bar, 0, 10);
    expectInRange(bar, 300, "drag on a zero-width bar");

    MoveWindow(bar, 0, 0, 600, 24, TRUE);
    expectClick(bar, 300, clientWidth(bar) / 2);
}

static void testZeroWidthPostsNothing(HWND bar)
{
    MoveWindow(bar, 0, 0, 600, 24, TRUE);
    seekbar::setRange(bar, 300);
    int sized_width = clientWidth(bar);
    check(sized_width > 0, "bar has a positive client width before shrinking it, got " + std::to_string(sized_width));

    SetWindowPos(bar, nullptr, 0, 0, 0, 24, SWP_NOZORDER | SWP_NOACTIVATE);
    int width = clientWidth(bar);
    check(width == 0, "SetWindowPos gives the bar a client width of 0, got " + std::to_string(width));

    for (int x : {0, 1, -1, 300, 32000}) {
        click(bar, x);
        expectNothing(bar, 300, "click at x=" + std::to_string(x) + " on a zero-width bar with a range set");
    }

    drag(bar, 0, 10);
    expectNothing(bar, 300, "drag from x=0 to x=10 on a zero-width bar");
    drag(bar, 32000, 0);
    expectNothing(bar, 300, "drag from x=32000 to x=0 on a zero-width bar");

    press(bar, WM_LBUTTONDOWN, MK_LBUTTON, 0, 0);
    press(bar, WM_MOUSEMOVE, MK_LBUTTON, 200, 0);
    press(bar, WM_LBUTTONUP, 0, 200, 0);
    expectNothing(bar, 300, "press, mouse move and release on a zero-width bar");

    seekbar::setRange(bar, 0);
    click(bar, 0);
    expectNothing(bar, 0, "click on a zero-width bar with no usable range");
    seekbar::setRange(bar, 300);

    seekbar::setPosition(bar, 120);
    paint(bar);
    expectNothing(bar, 300, "setPosition and repaint after a click on a zero-width bar");
}

static void testZeroWidthRecovery(HWND bar)
{
    seekbar::setRange(bar, 300);
    SetWindowPos(bar, nullptr, 0, 0, 0, 24, SWP_NOZORDER | SWP_NOACTIVATE);
    check(clientWidth(bar) == 0, "bar is zero-width before the recovery click, got " + std::to_string(clientWidth(bar)));
    click(bar, 0);
    drag(bar, 0, 50);
    expectNothing(bar, 300, "click and drag on a zero-width bar before resizing");

    MoveWindow(bar, 0, 0, 600, 24, TRUE);
    int width = clientWidth(bar);
    check(width > 0, "bar has a positive client width after the resize, got " + std::to_string(width));
    expectClick(bar, 300, 3 * width / 4);
    expectClick(bar, 300, width / 2);
    expectClick(bar, 300, 0);
}

static void testZeroWidthFreshBar(HINSTANCE instance)
{
    HWND fresh = seekbar::create(parent, instance);
    check(fresh != nullptr && IsWindow(fresh), "a fresh seek bar can be created for the zero-width case");
    if (!fresh || !IsWindow(fresh))
        return;

    SetWindowPos(fresh, nullptr, 0, 80, 0, 24, SWP_NOZORDER | SWP_NOACTIVATE);
    int width = clientWidth(fresh);
    check(width == 0, "a never-laid-out bar has a client width of 0, got " + std::to_string(width));

    seekbar::setRange(fresh, 300);
    for (int x : {0, 1, 7, 32000}) {
        click(fresh, x);
        expectNothing(fresh, 300, "click at x=" + std::to_string(x) + " on a never-laid-out bar");
    }
    drag(fresh, 0, 20);
    expectNothing(fresh, 300, "drag on a never-laid-out bar");

    MoveWindow(fresh, 0, 80, 600, 24, TRUE);
    int sized = clientWidth(fresh);
    check(sized > 0, "the fresh bar has a positive client width after layout, got " + std::to_string(sized));
    expectClick(fresh, 300, sized / 2);
    expectClick(fresh, 300, 3 * sized / 4);

    DestroyWindow(fresh);
}

static void testTwoBars(HINSTANCE instance, HWND bar)
{
    HWND other = seekbar::create(parent, instance);
    check(other != nullptr && IsWindow(other), "a second seek bar can be created");
    check(other != bar, "the second seek bar is a different window");
    if (!other || !IsWindow(other))
        return;
    MoveWindow(other, 0, 40, 600, 24, TRUE);
    seekbar::setRange(bar, 300);
    seekbar::setRange(other, 100);
    expectClick(other, 100, clientWidth(other) / 2);
    expectClick(bar, 300, clientWidth(bar) / 2);
    seekbar::setRange(other, 0);
    expectClick(bar, 300, clientWidth(bar) / 4);
    DestroyWindow(other);
}

static void testHover(HWND bar)
{
    MoveWindow(bar, 0, 0, 600, 24, TRUE);
    seekbar::setRange(bar, 300);
    seekbar::setMarks(bar, {}, 0);
    int width = clientWidth(bar);
    int y = middleY(bar);

    press(bar, WM_MOUSELEAVE, 0, 0, 0);
    check(seekbar::hoverText(bar).empty(), "hoverText is empty when no tooltip is shown");

    press(bar, WM_MOUSEMOVE, 0, 0, y);
    std::string at_zero = seekbar::hoverText(bar);
    check(!at_zero.empty(), "bare hover at x=0 produces a tooltip");
    check(at_zero == timecode::format(0), "hover at x=0 shows timecode::format(0), got \"" + at_zero + "\"");

    press(bar, WM_MOUSEMOVE, 0, width / 2, y);
    std::string at_mid = seekbar::hoverText(bar);
    check(!at_mid.empty(), "bare hover at x=width/2 produces a tooltip");
    int expected = static_cast<int>(std::lround(static_cast<double>(width / 2) * 300 / width));
    bool matched = false;
    for (int s = expected - 2; s <= expected + 2; ++s)
        if (at_mid == timecode::format(s))
            matched = true;
    check(matched, "hover at x=width/2 shows a timecode near " + timecode::format(expected) + ", got \"" + at_mid + "\"");

    press(bar, WM_MOUSEMOVE, 0, width / 2, y);
    check(!seekbar::hoverText(bar).empty(), "a tooltip is shown before WM_MOUSELEAVE");
    press(bar, WM_MOUSELEAVE, 0, 0, 0);
    check(seekbar::hoverText(bar).empty(), "hoverText is empty after WM_MOUSELEAVE");

    seekbar::setRange(bar, 0);
    press(bar, WM_MOUSEMOVE, 0, width / 2, y);
    check(seekbar::hoverText(bar).empty(), "hover with length 0 shows no tooltip");
    seekbar::setRange(bar, 300);

    press(bar, WM_LBUTTONDOWN, MK_LBUTTON, width / 4, y);
    press(bar, WM_MOUSEMOVE, MK_LBUTTON, width / 2, y);
    check(seekbar::hoverText(bar).empty(), "no tooltip is shown while dragging");
    press(bar, WM_LBUTTONUP, 0, width / 2, y);
    takeSeeks();

    std::vector<Bookmark> marks(1);
    marks[0].time = 150;
    marks[0].label = "intro";
    seekbar::setMarks(bar, marks, 0);
    press(bar, WM_MOUSEMOVE, 0, width / 2, y);
    std::string tip = seekbar::hoverText(bar);
    check(tip.find("intro") != std::string::npos, "hover on a bookmark tick includes its label, got \"" + tip + "\"");
    seekbar::setMarks(bar, {}, 0);
    takeSeeks();
}

int main()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = parentProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"SeekbarTestParent";
    RegisterClassW(&window_class);
    parent = CreateWindowExW(0, L"SeekbarTestParent", L"seekbar test", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             0, 0, 1400, 200, nullptr, nullptr, instance, nullptr);
    check(parent != nullptr, "hidden parent window is created");
    if (!parent)
        return report();

    HWND bar = seekbar::create(parent, instance);
    check(bar != nullptr && IsWindow(bar), "create() returns a window");
    if (!bar || !IsWindow(bar))
        return report();
    check(GetParent(bar) == parent, "create() returns a child of the parent");
    check((GetWindowLongPtrW(bar, GWL_STYLE) & WS_CHILD) != 0, "create() returns a WS_CHILD window");

    testClicks(bar);
    testDrags(bar);
    testMarksAndPosition(bar);
    testLengths(bar);
    testZeroLength(bar);
    testResize(bar);
    testZeroWidthPostsNothing(bar);
    testZeroWidthRecovery(bar);
    testZeroWidthFreshBar(instance);
    testTwoBars(instance, bar);
    testHover(bar);

    DestroyWindow(bar);
    DestroyWindow(parent);
    return report();
}
