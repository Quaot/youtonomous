#include "marks_panel.h"

#include <algorithm>

#include <commctrl.h>

#include "text.h"
#include "timecode.h"

using namespace ui;

namespace {

const int kNoteHeight = 64;

std::string replaceAll(std::string text, const std::string& from, const std::string& to)
{
    size_t at = 0;
    while ((at = text.find(from, at)) != std::string::npos) {
        text.replace(at, from.size(), to);
        at += to.size();
    }
    return text;
}

}

void MarksPanel::create(const Context& context)
{
    const DWORD list_style = WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT;
    const DWORD note_style = ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;

    start_label_ = addControl(context, L"STATIC", L"Start at", SS_LEFT, 0);
    start_edit_ = addControl(context, L"EDIT", L"", ES_AUTOHSCROLL, kStart, WS_EX_CLIENTEDGE);
    set_start_button_ = addControl(context, L"BUTTON", L"Save start", BS_PUSHBUTTON, kSetStart);
    go_start_button_ = addControl(context, L"BUTTON", L"Play from the start", BS_PUSHBUTTON, kGoStart);
    resume_button_ = addControl(context, L"BUTTON", L"Carry on", BS_PUSHBUTTON, kResume);

    marks_label_ = addControl(context, L"STATIC", L"Bookmarks", SS_LEFT, 0);
    list_ = addControl(context, L"LISTBOX", L"", list_style, kMarks, WS_EX_CLIENTEDGE);
    label_edit_ = addControl(context, L"EDIT", L"", ES_AUTOHSCROLL, kMarkLabel, WS_EX_CLIENTEDGE);
    note_edit_ = addControl(context, L"EDIT", L"", note_style, kMarkNote, WS_EX_CLIENTEDGE);
    add_button_ = addControl(context, L"BUTTON", L"Add", BS_PUSHBUTTON, kAddMark);
    edit_button_ = addControl(context, L"BUTTON", L"Edit", BS_PUSHBUTTON, kEditMark);
    delete_button_ = addControl(context, L"BUTTON", L"Delete", BS_PUSHBUTTON, kDeleteMark);

    SendMessageW(start_edit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"0:00"));
    SendMessageW(label_edit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Label (optional)"));
    SendMessageW(note_edit_, EM_SETREADONLY, TRUE, 0);
    showStoppedAt(0);
}

void MarksPanel::layout(int width, int height)
{
    int bottom = height - kPad - kRow;
    int left = width - kPad - kSide;
    int label_row_y = bottom - kGap - kRow;
    int note_y = label_row_y - kGap - kNoteHeight;
    int half = (kSide - kGap) / 2;

    int y = kPad;
    MoveWindow(start_label_, left, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(start_edit_, left, y, kSide - kSmallButton - kGap, kRow, TRUE);
    MoveWindow(set_start_button_, left + kSide - kSmallButton, y, kSmallButton, kRow, TRUE);
    y += kRow + kGap;
    MoveWindow(go_start_button_, left, y, half, kRow, TRUE);
    MoveWindow(resume_button_, left + half + kGap, y, half, kRow, TRUE);
    y += kRow + kPad;
    MoveWindow(marks_label_, left, y, kSide, kLabel, TRUE);
    y += kLabel;
    MoveWindow(list_, left, y, kSide, std::max(0, note_y - kGap - y), TRUE);
    MoveWindow(note_edit_, left, note_y, kSide, kNoteHeight, TRUE);
    MoveWindow(label_edit_, left, label_row_y, kSide - kSmallButton - kGap, kRow, TRUE);
    MoveWindow(add_button_, left + kSide - kSmallButton, label_row_y, kSmallButton, kRow, TRUE);
    MoveWindow(edit_button_, left, bottom, half, kRow, TRUE);
    MoveWindow(delete_button_, left + half + kGap, bottom, half, kRow, TRUE);
}

void MarksPanel::setVisible(bool visible)
{
    ui::setVisible({start_label_, start_edit_, set_start_button_, go_start_button_, resume_button_, marks_label_, list_,
                    label_edit_, note_edit_, add_button_, edit_button_, delete_button_},
                   visible);
}

void MarksPanel::show(const Video* video)
{
    SendMessageW(list_, LB_RESETCONTENT, 0, 0);
    if (!video) {
        setText(start_edit_, L"");
        return;
    }

    for (const Bookmark& mark : video->marks) {
        std::wstring line = widen(timecode::format(mark.time) + "  " + mark.label);
        SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
    }
    showStart(video->start);
}

void MarksPanel::showStart(int seconds)
{
    setText(start_edit_, widen(timecode::format(seconds)));
    setText(go_start_button_, seconds > 0 ? widen("Play from " + timecode::format(seconds)) : L"Play from the start");
}

void MarksPanel::showStoppedAt(int seconds)
{
    setText(start_label_, L"Skip the intro to");
    setText(resume_button_, seconds > 0 ? widen("Carry on from " + timecode::format(seconds)) : L"Carry on");
    EnableWindow(resume_button_, seconds > 0 ? TRUE : FALSE);
}

std::string MarksPanel::startText() const
{
    return narrow(textOf(start_edit_));
}

std::string MarksPanel::takeLabel()
{
    std::string label = trim(narrow(textOf(label_edit_)));
    SetWindowTextW(label_edit_, L"");
    return label;
}

int MarksPanel::selected(size_t count) const
{
    return selectedIndex(list_, count);
}

void MarksPanel::showNote(const std::string& note)
{
    setText(note_edit_, widen(replaceAll(replaceAll(note, "\r\n", "\n"), "\n", "\r\n")));
}

void MarksPanel::beginEdit(const std::string& label, const std::string& note)
{
    SetWindowTextW(label_edit_, widen(label).c_str());
    showNote(note);
    SendMessageW(note_edit_, EM_SETREADONLY, FALSE, 0);
    setText(add_button_, L"Save");
    setText(edit_button_, L"Cancel");
    SetFocus(label_edit_);
    SendMessageW(label_edit_, EM_SETSEL, 0, -1);
}

void MarksPanel::endEdit()
{
    SetWindowTextW(label_edit_, L"");
    SetWindowTextW(note_edit_, L"");
    SendMessageW(note_edit_, EM_SETREADONLY, TRUE, 0);
    setText(add_button_, L"Add");
    setText(edit_button_, L"Edit");
}

std::string MarksPanel::noteText() const
{
    return replaceAll(narrow(textOf(note_edit_)), "\r\n", "\n");
}
