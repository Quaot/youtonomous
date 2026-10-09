#pragma once

#include <initializer_list>
#include <string>

#include "player.h"

struct mpv_handle;

class MpvPlayer : public Player {
public:
    MpvPlayer();
    ~MpvPlayer() override;

    MpvPlayer(const MpvPlayer&) = delete;
    MpvPlayer& operator=(const MpvPlayer&) = delete;

    bool ready() const override { return mpv_ != nullptr; }
    bool loaded() const override { return loaded_; }

    void attach(HWND window) override;
    void open(const std::string& path, int start) override;
    void togglePause() override;

    bool playing() const override;
    int time() const override;
    int length() const override;

    void seek(int seconds) override;

    void setVolume(int percent) override;
    int volume() const override { return volume_; }
    void setSpeed(double rate) override;
    double speed() const override { return speed_; }

    bool pitchSupported() const override { return true; }
    void setPitch(int semitones) override;
    int pitch() const override { return pitch_; }
    void setKeepPitch(bool keep) override;
    bool keepPitch() const override { return keep_pitch_; }

private:
    void initialize();
    void applyVolumeAndSpeed();
    void applyPitch();
    void drainEvents() const;
    bool flag(const char* name) const;
    double number(const char* name) const;
    void command(std::initializer_list<const char*> args) const;

    mpv_handle* mpv_ = nullptr;
    bool initialized_ = false;
    bool loaded_ = false;
    int volume_ = 100;
    double speed_ = 1.0;
    int pitch_ = 0;
    bool keep_pitch_ = true;
};
