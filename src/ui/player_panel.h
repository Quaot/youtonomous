#pragma once

#include <string>
#include <vector>

#include <windows.h>

#include "controls.h"
#include "library.h"

class PlayerPanel {
public:
    void create(const ui::Context& context);
    void layout(int width, int height);
    void setFullscreen(bool fullscreen);

    HWND videoWindow() const { return video_; }
    void show(int time, int length, bool playing);
    void showMarks(const std::vector<Bookmark>& marks, int start);

    void showVolume(int percent);
    int volumeSetting() const;
    bool isVolumeBar(HWND window) const { return window == volume_bar_; }

    void showSpeed(double rate);
    double speedSetting() const;

    void showSession(const std::wstring& block, const std::wstring& totals);

    void setPitchAvailable(bool available);
    void showPitch(int semitones);
    int pitchSetting() const;
    void showKeepPitch(bool keep);
    bool keepPitchSetting() const;

private:
    HWND video_ = nullptr;
    HWND seekbar_ = nullptr;
    HWND prev_mark_ = nullptr;
    HWND back30_ = nullptr;
    HWND back10_ = nullptr;
    HWND play_button_ = nullptr;
    HWND forward10_ = nullptr;
    HWND forward30_ = nullptr;
    HWND next_mark_ = nullptr;
    HWND time_label_ = nullptr;
    HWND speed_box_ = nullptr;
    HWND pitch_box_ = nullptr;
    HWND keep_pitch_box_ = nullptr;
    HWND volume_bar_ = nullptr;
    HWND session_label_ = nullptr;
    HWND totals_label_ = nullptr;
    bool fullscreen_ = false;
    bool keep_pitch_shown_ = true;
};
