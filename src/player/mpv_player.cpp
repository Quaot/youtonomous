#include "mpv_player.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <mpv/client.h>

namespace {

// Loading libmpv costs a few percent of a core even when nothing plays, so the
// library is only brought in when mpv is the chosen backend.
struct MpvApi {
    mpv_handle* (*create)() = nullptr;
    int (*initialize)(mpv_handle*) = nullptr;
    void (*terminate_destroy)(mpv_handle*) = nullptr;
    int (*set_option)(mpv_handle*, const char*, mpv_format, void*) = nullptr;
    int (*set_option_string)(mpv_handle*, const char*, const char*) = nullptr;
    int (*set_property)(mpv_handle*, const char*, mpv_format, void*) = nullptr;
    int (*set_property_string)(mpv_handle*, const char*, const char*) = nullptr;
    int (*get_property)(mpv_handle*, const char*, mpv_format, void*) = nullptr;
    int (*command)(mpv_handle*, const char**) = nullptr;
    mpv_event* (*wait_event)(mpv_handle*, double) = nullptr;
    bool ok = false;
};

template <typename Function>
void bind(HMODULE library, Function& target, const char* name, bool& ok)
{
    target = reinterpret_cast<Function>(reinterpret_cast<void*>(GetProcAddress(library, name)));
    if (!target)
        ok = false;
}

MpvApi loadApi()
{
    MpvApi api;
    HMODULE library = LoadLibraryW(L"libmpv-2.dll");
    if (!library)
        library = LoadLibraryW(L"mpv-2.dll");
    if (!library)
        return api;

    api.ok = true;
    bind(library, api.create, "mpv_create", api.ok);
    bind(library, api.initialize, "mpv_initialize", api.ok);
    bind(library, api.terminate_destroy, "mpv_terminate_destroy", api.ok);
    bind(library, api.set_option, "mpv_set_option", api.ok);
    bind(library, api.set_option_string, "mpv_set_option_string", api.ok);
    bind(library, api.set_property, "mpv_set_property", api.ok);
    bind(library, api.set_property_string, "mpv_set_property_string", api.ok);
    bind(library, api.get_property, "mpv_get_property", api.ok);
    bind(library, api.command, "mpv_command", api.ok);
    bind(library, api.wait_event, "mpv_wait_event", api.ok);
    return api;
}

const MpvApi& mpv()
{
    static MpvApi api = loadApi();
    return api;
}

}

MpvPlayer::MpvPlayer()
{
    if (!mpv().ok)
        return;

    mpv_ = mpv().create();
    if (!mpv_)
        return;

    mpv().set_option_string(mpv_, "config", "no");
    mpv().set_option_string(mpv_, "terminal", "no");
    mpv().set_option_string(mpv_, "idle", "yes");
    mpv().set_option_string(mpv_, "keep-open", "yes");
    mpv().set_option_string(mpv_, "hr-seek", "yes");
    mpv().set_option_string(mpv_, "hwdec", "auto-safe");
    mpv().set_option_string(mpv_, "osc", "no");
    mpv().set_option_string(mpv_, "osd-level", "0");
    mpv().set_option_string(mpv_, "input-default-bindings", "no");
    mpv().set_option_string(mpv_, "input-vo-keyboard", "no");
    mpv().set_option_string(mpv_, "input-cursor", "no");
}

MpvPlayer::~MpvPlayer()
{
    if (mpv_)
        mpv().terminate_destroy(mpv_);
}

void MpvPlayer::initialize()
{
    if (!mpv_ || initialized_)
        return;
    initialized_ = mpv().initialize(mpv_) >= 0;
}

void MpvPlayer::attach(HWND window)
{
    if (!mpv_ || initialized_)
        return;

    int64_t wid = static_cast<int64_t>(reinterpret_cast<intptr_t>(window));
    mpv().set_option(mpv_, "wid", MPV_FORMAT_INT64, &wid);
    initialize();
}

void MpvPlayer::open(const std::string& path, int start)
{
    initialize();
    if (!initialized_)
        return;

    std::string start_text = std::to_string(std::max(start, 0));
    mpv().set_property_string(mpv_, "start", start_text.c_str());
    mpv().set_property_string(mpv_, "pause", "no");
    applyVolumeAndSpeed();
    applyPitch();
    command({"loadfile", path.c_str(), "replace"});
    loaded_ = true;
}

void MpvPlayer::togglePause()
{
    if (!loaded_)
        return;

    if (flag("eof-reached")) {
        command({"seek", "0", "absolute+exact"});
        mpv().set_property_string(mpv_, "pause", "no");
        return;
    }
    command({"cycle", "pause"});
}

bool MpvPlayer::playing() const
{
    if (!loaded_)
        return false;
    return !flag("pause") && !flag("idle-active") && !flag("eof-reached");
}

int MpvPlayer::time() const
{
    if (!loaded_)
        return 0;
    return static_cast<int>(std::max(0.0, number("time-pos")));
}

int MpvPlayer::length() const
{
    if (!loaded_)
        return 0;
    return static_cast<int>(std::max(0.0, number("duration")));
}

void MpvPlayer::seek(int seconds)
{
    if (!loaded_)
        return;

    int end = length();
    if (end > 0)
        seconds = std::min(seconds, end - 1);
    seconds = std::max(seconds, 0);

    std::string target = std::to_string(seconds);
    command({"seek", target.c_str(), "absolute+exact"});
}

void MpvPlayer::setVolume(int percent)
{
    volume_ = std::clamp(percent, 0, 100);
    applyVolumeAndSpeed();
}

void MpvPlayer::setSpeed(double rate)
{
    if (!std::isfinite(rate))
        return;
    speed_ = std::clamp(rate, 0.5, 2.0);
    applyVolumeAndSpeed();
}

void MpvPlayer::setPitch(int semitones)
{
    pitch_ = std::clamp(semitones, -6, 6);
    applyPitch();
}

void MpvPlayer::setKeepPitch(bool keep)
{
    keep_pitch_ = keep;
    applyPitch();
}

void MpvPlayer::applyPitch()
{
    if (!mpv_)
        return;

    mpv().set_property_string(mpv_, "audio-pitch-correction", keep_pitch_ ? "yes" : "no");

    if (pitch_ == 0) {
        mpv().set_property_string(mpv_, "af", "");
        return;
    }

    char filter[64];
    std::snprintf(filter, sizeof filter, "rubberband=pitch-scale=%.6f", std::pow(2.0, pitch_ / 12.0));
    mpv().set_property_string(mpv_, "af", filter);
}

void MpvPlayer::applyVolumeAndSpeed()
{
    if (!mpv_)
        return;
    double volume = volume_;
    double speed = speed_;
    mpv().set_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &volume);
    mpv().set_property(mpv_, "speed", MPV_FORMAT_DOUBLE, &speed);
}

void MpvPlayer::drainEvents() const
{
    while (mpv().wait_event(mpv_, 0)->event_id != MPV_EVENT_NONE) {
    }
}

bool MpvPlayer::flag(const char* name) const
{
    drainEvents();
    int value = 0;
    if (mpv().get_property(mpv_, name, MPV_FORMAT_FLAG, &value) < 0)
        return false;
    return value != 0;
}

double MpvPlayer::number(const char* name) const
{
    drainEvents();
    double value = 0;
    if (mpv().get_property(mpv_, name, MPV_FORMAT_DOUBLE, &value) < 0 || !std::isfinite(value))
        return 0;
    return value;
}

void MpvPlayer::command(std::initializer_list<const char*> args) const
{
    std::vector<const char*> list(args);
    list.push_back(nullptr);
    mpv().command(mpv_, list.data());
}
