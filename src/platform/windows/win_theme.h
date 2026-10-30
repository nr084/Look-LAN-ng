#ifndef WIN_THEME_H
#define WIN_THEME_H
#include <windows.h>
#include <commctrl.h>
#include <stdbool.h>
typedef enum { WIN_THEME_SYSTEM,WIN_THEME_LIGHT,WIN_THEME_DARK } win_theme_mode_t;
typedef enum { THEME_PANEL,THEME_SURFACE,THEME_TEXT,THEME_MUTED,THEME_HOVER,THEME_BORDER,THEME_ACCENT } win_theme_color_t;
void win_theme_set(win_theme_mode_t mode);
win_theme_mode_t win_theme_mode(void);
bool win_theme_dark(void);
COLORREF win_theme_color(win_theme_color_t color);
HBRUSH win_theme_brush(win_theme_color_t color);
void win_theme_apply(HWND window,HFONT font);
void win_theme_menu(HMENU menu,bool menu_bar);
void win_theme_menu_edge(HWND window,HDC target);
void win_theme_destroy_menu(HMENU menu);
bool win_theme_message(HWND window,UINT message,WPARAM w,LPARAM l,LRESULT *result);
void win_theme_cleanup(void);
#endif
