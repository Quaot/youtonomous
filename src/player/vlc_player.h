#pragma once

#include "player.h"

struct libvlc_instance_t;
struct libvlc_media_player_t;

class VlcPlayer : public Player {
public:
    VlcPlayer();
    ~VlcPlayer() override;

    VlcPlayer(const VlcPlayer&) = delete;
    VlcPlayer& operator=(const VlcPlayer&) = delete;

    bool ready() const override { return player_ != nullptr; }
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

    // libvlc gives no way to reach the pitch of its scaletempo filter.
    bool pitchSupported() const override { return false; }
    void setPitch(int) override {}
    int pitch() const override { return 0; }
    void setKeepPitch(bool) override {}
    bool keepPitch() const override { return true; }

private:
    void applyVolumeAndSpeed();

    libvlc_instance_t* vlc_ = nullptr;
    libvlc_media_player_t* player_ = nullptr;
    bool loaded_ = false;
    mutable int seek_target_ = -1;
    mutable unsigned long long seek_tick_ = 0;
    int volume_ = 100;
    double speed_ = 1.0;
};
