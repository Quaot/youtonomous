#include "player_panel.h"

#include <algorithm>
#include <cmath>

#include <commctrl.h>

#include "seekbar.h"
#include "text.h"
#include "timecode.h"

using namespace ui;

namespace {

const double kSpeeds[] = {0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0};
const wchar_t* kSpeedNames[] = {L"0.5x", L"0.75x", L"1x", L"1.25x", L"1.5x", L"1.75x", L"2x"};
const int kSpeedCount = 7;

const wchar_t* kPitchNames[] = {L"-6", L"-5", L"-4", L"-3", L"-2", L"-1", L"0",
                                L"+1", L"+2", L"+3", L"+4", L"+5", L"+6"};
const int kPitchCount = 13;
const int kLowestPitch = -6;

const int kTimeWidth = 100;
const int kSpeedWidth = 58;
const int kPitchWidth = 50;
const int kVolumeWidth = 80;
const int kKeepPitchWidth = 96;

}

void PlayerPanel::create(const Context& context)
{
    video_ = addControl(context, L"STATIC", L"", SS_BLACKRECT | SS_NOTIFY, kVideo);
    seekbar_ = seekbar::create(context.parent, context.instance);
    prev_mark_ = addControl(context, L"BUTTON", L"Prev", BS_PUSHBUTTON, kPrevMark);
    back30_ = addControl(context, L"BUTTON", L"-30", BS_PUSHBUTTON, kBack30);
    back10_ = addControl(context, L"BUTTON", L"-10", BS_PUSHBUTTON, kBack10);
    play_button_ = addControl(context, L"BUTTON", L"Play", BS_PUSHBUTTON, kPlay);
    forward10_ = addControl(context, L"BUTTON", L"+10", BS_PUSHBUTTON, kForward10);
    forward30_ = addControl(context, L"BUTTON", L"+30", BS_PUSHBUTTON, kForward30);
    next_mark_ = addControl(context, L"BUTTON", L"Next", BS_PUSHBUTTON, kNextMark);
    time_label_ = addControl(context, L"STATIC", L"0:00 / 0:00", SS_CENTERIMAGE, 0);

    speed_box_ = addControl(context, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, kSpeed);
    for (const wchar_t* name : kSpeedNames)
        SendMessageW(speed_box_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));

    pitch_box_ = addControl(context, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, kPitch);
    for (const wchar_t* name : kPitchNames)
        SendMessageW(pitch_box_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));

    keep_pitch_box_ = addControl(context, L"BUTTON", L"Keep pitch", BS_AUTOCHECKBOX, kKeepPitch);

    volume_bar_ = addControl(context, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS, kVolume);
    SendMessageW(volume_bar_, TBM_SETRANGEMIN, FALSE, 0);
    SendMessageW(volume_bar_, TBM_SETRANGEMAX, TRUE, 100);

    showSpeed(1.0);
    showPitch(0);
    showKeepPitch(true);
    showVolume(100);
}

void PlayerPanel::layout(int width, int height)
{
    if (fullscreen_) {
        MoveWindow(video_, 0, 0, width, height, TRUE);
        return;
    }

    int bottom = height - kPad - kRow;
    int left = kSide + kPad * 2;
    int center_width = std::max(0, width - kSide * 2 - kPad * 4);
    int right = left + center_width;
    int seek_y = bottom - kPad - kSeekHeight;

    MoveWindow(video_, left, kPad, center_width, std::max(0, seek_y - kPad * 2), TRUE);
    MoveWindow(seekbar_, left, seek_y, center_width, kSeekHeight, TRUE);

    int x = left;
    for (HWND button : {prev_mark_, back30_, back10_, play_button_, forward10_, forward30_, next_mark_}) {
        MoveWindow(button, x, bottom, kButton, kRow, TRUE);
        x += kButton + kGap;
    }

    int buttons_end = x - kGap;
    int cluster = kSpeedWidth + kGap + kPitchWidth + kGap + kVolumeWidth + kGap + kKeepPitchWidth;
    keep_pitch_shown_ = right - cluster - buttons_end - kPad >= kTimeWidth;
    if (!keep_pitch_shown_)
        cluster -= kGap + kKeepPitchWidth;
    ShowWindow(keep_pitch_box_, keep_pitch_shown_ ? SW_SHOW : SW_HIDE);

    int cluster_x = std::max(buttons_end + kPad, right - cluster);
    int label_x = buttons_end + kPad;
    MoveWindow(time_label_, label_x, bottom, std::max(0, cluster_x - kGap - label_x), kRow, TRUE);

    x = cluster_x;
    MoveWindow(speed_box_, x, bottom, kSpeedWidth, 200, TRUE);
    x += kSpeedWidth + kGap;
    MoveWindow(pitch_box_, x, bottom, kPitchWidth, 200, TRUE);
    x += kPitchWidth + kGap;
    MoveWindow(volume_bar_, x, bottom, kVolumeWidth, kRow, TRUE);
    x += kVolumeWidth + kGap;
    if (keep_pitch_shown_)
        MoveWindow(keep_pitch_box_, x, bottom, kKeepPitchWidth, kRow, TRUE);
}

void PlayerPanel::setFullscreen(bool fullscreen)
{
    fullscreen_ = fullscreen;
    ui::setVisible({seekbar_, prev_mark_, back30_, back10_, play_button_, forward10_, forward30_, next_mark_,
                    time_label_, speed_box_, pitch_box_, volume_bar_},
                   !fullscreen);
    ShowWindow(keep_pitch_box_, !fullscreen && keep_pitch_shown_ ? SW_SHOW : SW_HIDE);
}

void PlayerPanel::show(int time, int length, bool playing)
{
    seekbar::setRange(seekbar_, length);
    seekbar::setPosition(seekbar_, time);

    setText(time_label_, widen(timecode::format(time) + " / " + timecode::format(length)));
    setText(play_button_, playing ? L"Pause" : L"Play");
}

void PlayerPanel::showMarks(const std::vector<Bookmark>& marks, int start)
{
    seekbar::setMarks(seekbar_, marks, start);
}

void PlayerPanel::showVolume(int percent)
{
    SendMessageW(volume_bar_, TBM_SETPOS, TRUE, std::clamp(percent, 0, 100));
}

int PlayerPanel::volumeSetting() const
{
    return static_cast<int>(SendMessageW(volume_bar_, TBM_GETPOS, 0, 0));
}

void PlayerPanel::showSpeed(double rate)
{
    int nearest = 0;
    for (int i = 1; i < kSpeedCount; ++i) {
        if (std::fabs(kSpeeds[i] - rate) < std::fabs(kSpeeds[nearest] - rate))
            nearest = i;
    }
    SendMessageW(speed_box_, CB_SETCURSEL, nearest, 0);
}

double PlayerPanel::speedSetting() const
{
    int index = static_cast<int>(SendMessageW(speed_box_, CB_GETCURSEL, 0, 0));
    if (index < 0 || index >= kSpeedCount)
        return 1.0;
    return kSpeeds[index];
}

void PlayerPanel::setPitchAvailable(bool available)
{
    EnableWindow(pitch_box_, available ? TRUE : FALSE);
    EnableWindow(keep_pitch_box_, available ? TRUE : FALSE);
}

void PlayerPanel::showPitch(int semitones)
{
    int index = std::clamp(semitones, kLowestPitch, kLowestPitch + kPitchCount - 1) - kLowestPitch;
    SendMessageW(pitch_box_, CB_SETCURSEL, index, 0);
}

int PlayerPanel::pitchSetting() const
{
    int index = static_cast<int>(SendMessageW(pitch_box_, CB_GETCURSEL, 0, 0));
    if (index < 0 || index >= kPitchCount)
        return 0;
    return kLowestPitch + index;
}

void PlayerPanel::showKeepPitch(bool keep)
{
    SendMessageW(keep_pitch_box_, BM_SETCHECK, keep ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool PlayerPanel::keepPitchSetting() const
{
    return SendMessageW(keep_pitch_box_, BM_GETCHECK, 0, 0) == BST_CHECKED;
}
