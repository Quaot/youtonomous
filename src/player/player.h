#pragma once

#include <string>

#include <windows.h>

class Player {
public:
    virtual ~Player() = default;

    virtual bool ready() const = 0;
    virtual bool loaded() const = 0;

    virtual void attach(HWND window) = 0;
    virtual void open(const std::string& path, int start) = 0;
    virtual void togglePause() = 0;

    virtual bool playing() const = 0;
    virtual int time() const = 0;
    virtual int length() const = 0;

    virtual void seek(int seconds) = 0;
    void skip(int seconds) { seek(time() + seconds); }

    virtual void setVolume(int percent) = 0;
    virtual int volume() const = 0;
    virtual void setSpeed(double rate) = 0;
    virtual double speed() const = 0;

    virtual bool pitchSupported() const = 0;
    virtual void setPitch(int semitones) = 0;
    virtual int pitch() const = 0;
    virtual void setKeepPitch(bool keep) = 0;
    virtual bool keepPitch() const = 0;
};
