#ifndef WIN_TOOLBAR_ICONS_H
#define WIN_TOOLBAR_ICONS_H
#include <windows.h>
#include <stdbool.h>
/* Original geometric glyphs, rendered at 4x for smooth native-DPI edges. */
void win_toolbar_icon(HDC target, int x, int y, int size, unsigned icon, bool enabled, COLORREF background);
#endif
