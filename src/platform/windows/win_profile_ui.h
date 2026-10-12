#ifndef WIN_PROFILE_UI_H
#define WIN_PROFILE_UI_H
#include <windows.h>
#include "profile.h"
#include "report.h"
bool win_profile_edit(HWND owner, bool russian, profile_t *profile);
bool win_profile_file_dialog(HWND owner, bool save, bool russian, wchar_t *path);
bool win_report_file_dialog(HWND owner, report_format_t format, bool russian, wchar_t *path);
bool win_profile_ui_smoke(HWND owner);
#endif
