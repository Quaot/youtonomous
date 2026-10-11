#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include <windows.h>

#include "downloader.h"
#include "library.h"
#include "library_panel.h"
#include "make_player.h"
#include "marks_panel.h"
#include "player_panel.h"
#include "session.h"
#include "settings.h"
#include "study_log.h"

class MainWindow {
public:
    MainWindow();

    bool create(HINSTANCE instance, int show);
    void openPath(const std::wstring& path);
    bool handleKey(const MSG& msg);

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param);
    LRESULT handle(UINT msg, WPARAM w_param, LPARAM l_param);

    void createControls();
    void layout(int width, int height);
    void onCommand(int id, int code);
    void tick();
    void saveSettingsOnClose();
    void setFullscreen(bool fullscreen);

    void openFile();
    void download();
    void onDownloadDone(DownloadResult* result);
    void refreshLibrary();
    void openVideo(const std::string& id);
    void removeSelected();

    Video* current();
    void refreshMarks();
    void addMark();
    void deleteSelectedMark();
    void jumpToSelectedMark();
    void jumpMark(int direction);
    void showSelectedNote();
    void startEditingMark();
    void saveEditedMark();
    void cancelEditingMark();

    void saveStartFromBox();
    void setStart(int seconds);
    void goToStart();

    void changeSpeed(double step);
    void rememberPosition();
    void resume();

    void updateSession();
    void skipBlock();
    void applyVolume();
    void recordStudy(const std::string& label);
    std::string currentBlockLabel() const;
    bool currentBlockIsRest() const;
    std::string studyLabel() const;
    void refreshTotals();
    std::wstring totalsText() const;

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HFONT font_ = nullptr;

    LibraryPanel library_panel_;
    PlayerPanel player_panel_;
    MarksPanel marks_panel_;

    bool fullscreen_ = false;
    WINDOWPLACEMENT saved_placement_{};
    LONG_PTR saved_style_ = 0;

    std::filesystem::path settings_file_;
    Settings settings_;
    std::filesystem::path videos_folder_;
    Library library_;
    std::unique_ptr<Player> player_;
    std::string current_id_;
    int editing_mark_ = -1;
    bool downloading_ = false;

    std::filesystem::path study_file_;
    std::vector<Block> blocks_;
    SessionState session_;
    unsigned long long last_tick_ = 0;
    double block_studied_ = 0;
    int studied_today_ = 0;
    int studied_total_ = 0;
    int wanted_volume_ = 100;
    int applied_volume_ = -1;
};
