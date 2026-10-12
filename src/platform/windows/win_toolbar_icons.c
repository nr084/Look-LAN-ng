#include "win_toolbar_icons.h"

static void line(HDC dc, int x1, int y1, int x2, int y2) {
    MoveToEx(dc,x1,y1,NULL); LineTo(dc,x2,y2);
}
void win_toolbar_icon(HDC target, int x, int y, int size, unsigned icon, bool enabled, COLORREF background) {
    if (size <= 0 || icon > 5) return;
    HDC dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target,size*4,size*4);
    if (!dc || !bitmap) { if (dc) DeleteDC(dc); if (bitmap) DeleteObject(bitmap); return; }
    HGDIOBJ old_bitmap = SelectObject(dc,bitmap);
    RECT bounds = {0,0,size*4,size*4}; HBRUSH bg = CreateSolidBrush(background);
    FillRect(dc,&bounds,bg); DeleteObject(bg);
    SetMapMode(dc,MM_ANISOTROPIC); SetWindowExtEx(dc,24,24,NULL); SetViewportExtEx(dc,size*4,size*4,NULL);
    COLORREF ink = enabled ? RGB(51,75,88) : GetSysColor(COLOR_GRAYTEXT);
    COLORREF accent = enabled ? (icon == 1 ? RGB(196,82,65) : RGB(20,145,148)) : ink;
    HPEN outline = CreatePen(PS_SOLID,2,ink), colored = CreatePen(PS_SOLID,2,accent);
    HBRUSH fill = CreateSolidBrush(accent);
    HGDIOBJ old_pen = SelectObject(dc,outline), old_brush = SelectObject(dc,GetStockObject(NULL_BRUSH));
    switch (icon) {
    case 0: /* Discovery: radar ring and sweep. */
        Ellipse(dc,3,3,21,21); SelectObject(dc,colored); Ellipse(dc,7,7,17,17);
        line(dc,12,12,19,5); SelectObject(dc,fill); Ellipse(dc,10,10,14,14); break;
    case 1: /* Stop: rounded, restrained red square. */
        SelectObject(dc,colored); SelectObject(dc,fill); RoundRect(dc,5,5,19,19,4,4); break;
    case 2: /* Refresh: open circle with directional arrow. */
        SelectObject(dc,colored); Arc(dc,4,4,20,20,12,3,20,10);
        line(dc,19,4,19,10); line(dc,19,10,13,10); break;
    case 3: /* Single host: terminal and small discovery lens. */
        RoundRect(dc,2,4,17,15,3,3); line(dc,9,15,9,19); line(dc,5,19,13,19);
        SelectObject(dc,colored); Ellipse(dc,13,12,21,20); line(dc,19,19,23,23); break;
    case 4: /* Monitoring: screen with live signal. */
        RoundRect(dc,2,4,22,20,3,3); SelectObject(dc,colored);
        POINT signal[] = {{4,13},{8,13},{10,8},{13,17},{16,11},{20,11}}; Polyline(dc,signal,6); break;
    case 5: /* Export: downward arrow into a document tray. */
        line(dc,4,14,4,21); line(dc,4,21,20,21); line(dc,20,21,20,14);
        SelectObject(dc,colored); line(dc,12,3,12,16); line(dc,7,11,12,16); line(dc,12,16,17,11); break;
    }
    SelectObject(dc,old_pen); SelectObject(dc,old_brush);
    DeleteObject(outline); DeleteObject(colored); DeleteObject(fill);
    SetMapMode(dc,MM_TEXT);
    int saved = SaveDC(target); SetStretchBltMode(target,HALFTONE); SetBrushOrgEx(target,0,0,NULL);
    StretchBlt(target,x,y,size,size,dc,0,0,size*4,size*4,SRCCOPY); RestoreDC(target,saved);
    SelectObject(dc,old_bitmap); DeleteObject(bitmap); DeleteDC(dc);
}
