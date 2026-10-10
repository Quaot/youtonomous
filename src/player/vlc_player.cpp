#include "vlc_player.h"

#include <algorithm>
#include <cmath>

#include <vlc/vlc.h>

VlcPlayer::VlcPlayer()
{
    const char* args[] = {"--no-video-title-show"};
    vlc_ = libvlc_new(1, args);
    if (vlc_)
        player_ = libvlc_media_player_new(vlc_);
}

VlcPlayer::~VlcPlayer()
{
    if (player_) {
        libvlc_media_player_stop(player_);
        libvlc_media_player_release(player_);
    }
    if (vlc_)
        libvlc_release(vlc_);
}

void VlcPlayer::attach(HWND window)
{
    if (!ready())
        return;
    libvlc_media_player_set_hwnd(player_, window);
    libvlc_video_set_mouse_input(player_, false);
    libvlc_video_set_key_input(player_, false);
}

void VlcPlayer::open(const std::string& path, int start)
{
    if (!ready())
        return;

    libvlc_media_t* media = libvlc_media_new_path(vlc_, path.c_str());
    if (!media)
        return;

    std::string option = ":start-time=" + std::to_string(start);
    libvlc_media_add_option(media, option.c_str());
    libvlc_media_player_set_media(player_, media);
    libvlc_media_release(media);

    libvlc_media_player_play(player_);
    loaded_ = true;
    applyVolumeAndSpeed();
}

void VlcPlayer::togglePause()
{
    if (!loaded_)
        return;

    if (libvlc_media_player_get_state(player_) == libvlc_Ended) {
        libvlc_media_player_stop(player_);
        libvlc_media_player_play(player_);
        applyVolumeAndSpeed();
        return;
    }
    libvlc_media_player_pause(player_);
}

bool VlcPlayer::playing() const
{
    return loaded_ && libvlc_media_player_is_playing(player_);
}

int VlcPlayer::time() const
{
    if (!loaded_)
        return 0;

    int now = static_cast<int>(std::max<libvlc_time_t>(0, libvlc_media_player_get_time(player_)) / 1000);
    if (seek_target_ < 0)
        return now;

    // For about half a second after a seek VLC reports zero, or the time it
    // held before the seek, so follow the expected position until it settles.
    // While paused its clock can stay stale for as long as the pause lasts.
    if (!playing()) {
        if (std::abs(now - seek_target_) <= 1) {
            seek_target_ = -1;
            return now;
        }
        return seek_target_;
    }

    double elapsed = (GetTickCount64() - seek_tick_) / 1000.0;
    if (elapsed > 2.0) {
        seek_target_ = -1;
        return now;
    }

    int expected = seek_target_ + static_cast<int>(elapsed * speed_);
    int end = length();
    if (end > 0)
        expected = std::min(expected, end - 1);
    return std::abs(now - expected) > 1 ? expected : now;
}

int VlcPlayer::length() const
{
    if (!loaded_)
        return 0;
    return static_cast<int>(std::max<libvlc_time_t>(0, libvlc_media_player_get_length(player_)) / 1000);
}

void VlcPlayer::seek(int seconds)
{
    if (!loaded_)
        return;

    int end = length();
    if (end > 0)
        seconds = std::min(seconds, end - 1);
    seconds = std::max(seconds, 0);
    seek_target_ = seconds;
    seek_tick_ = GetTickCount64();
    libvlc_media_player_set_time(player_, static_cast<libvlc_time_t>(seconds) * 1000);
}

void VlcPlayer::setVolume(int percent)
{
    volume_ = std::clamp(percent, 0, 100);
    applyVolumeAndSpeed();
}

void VlcPlayer::setSpeed(double rate)
{
    if (!std::isfinite(rate))
        return;
    speed_ = std::clamp(rate, 0.5, 2.0);
    applyVolumeAndSpeed();
}

void VlcPlayer::applyVolumeAndSpeed()
{
    if (!ready())
        return;
    libvlc_audio_set_volume(player_, volume_);
    libvlc_media_player_set_rate(player_, static_cast<float>(speed_));
}
