#include "win_theme.h"
#include <uxtheme.h>
#include <dwmapi.h>
#include <oleacc.h>
#include <stdlib.h>
#include <wchar.h>
static win_theme_mode_t preference;
static bool dark;
static HFONT ui_font;
static HBRUSH brushes[7];
static const COLORREF palette[]={RGB(30,34,40),RGB(23,27,32),RGB(232,237,242),RGB(155,166,179),RGB(46,62,73),RGB(65,74,85),RGB(61,190,190)};
void win_theme_set(win_theme_mode_t mode){
    preference=mode<=WIN_THEME_DARK?mode:WIN_THEME_SYSTEM;
    DWORD light=1,size=sizeof(light);RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,NULL,&light,&size);
    HIGHCONTRASTW hc={sizeof(hc)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0);
    dark=!(hc.dwFlags&HCF_HIGHCONTRASTON) && (preference==WIN_THEME_DARK || (preference==WIN_THEME_SYSTEM && !light));
}
win_theme_mode_t win_theme_mode(void){return preference;}
bool win_theme_dark(void){return dark;}
COLORREF win_theme_color(win_theme_color_t color){static const int system[]={COLOR_BTNFACE,COLOR_WINDOW,COLOR_WINDOWTEXT,COLOR_GRAYTEXT,COLOR_HIGHLIGHT,COLOR_BTNSHADOW,COLOR_HIGHLIGHT};return dark?palette[color]:GetSysColor(system[color]);}
HBRUSH win_theme_brush(win_theme_color_t color){if(!dark){static const int system[]={COLOR_BTNFACE,COLOR_WINDOW,COLOR_WINDOWTEXT,COLOR_GRAYTEXT,COLOR_HIGHLIGHT,COLOR_BTNSHADOW,COLOR_HIGHLIGHT};return GetSysColorBrush(system[color]);}if(!brushes[color])brushes[color]=CreateSolidBrush(palette[color]);return brushes[color];}
static int scale(HWND h,int value){return MulDiv(value,(int)GetDpiForWindow(h),96);}
static void fill(HDC dc,const RECT *r,COLORREF color){SetDCBrushColor(dc,color);FillRect(dc,r,(HBRUSH)GetStockObject(DC_BRUSH));}
static void text(HDC dc,const wchar_t *s,RECT *r,UINT flags){SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s,-1,r,DT_SINGLELINE|DT_VCENTER|flags);}
static bool custom_draw(NMHDR *hdr,LRESULT *result){
    if(!dark || hdr->code!=NM_CUSTOMDRAW)return false;
    wchar_t cls[64];GetClassNameW(hdr->hwndFrom,cls,64);NMCUSTOMDRAW *draw=(NMCUSTOMDRAW *)hdr;
    if(!wcscmp(cls,WC_LISTVIEWW)){
        if(draw->dwDrawStage==CDDS_PREPAINT){*result=CDRF_NOTIFYITEMDRAW;return true;}
        if(draw->dwDrawStage==CDDS_ITEMPREPAINT){NMLVCUSTOMDRAW *v=(NMLVCUSTOMDRAW *)hdr;bool selected=(ListView_GetItemState(hdr->hwndFrom,(int)draw->dwItemSpec,LVIS_SELECTED)&LVIS_SELECTED)!=0;v->clrText=win_theme_color(THEME_TEXT);v->clrTextBk=win_theme_color(selected?THEME_HOVER:THEME_SURFACE);*result=CDRF_NEWFONT;return true;}
    }else if(!wcscmp(cls,WC_TREEVIEWW)){
        if(draw->dwDrawStage==CDDS_PREPAINT){*result=CDRF_NOTIFYITEMDRAW;return true;}
        if(draw->dwDrawStage==CDDS_ITEMPREPAINT){NMTVCUSTOMDRAW *v=(NMTVCUSTOMDRAW *)hdr;v->clrText=win_theme_color(THEME_TEXT);v->clrTextBk=win_theme_color((draw->uItemState&CDIS_SELECTED)?THEME_HOVER:THEME_SURFACE);*result=CDRF_NEWFONT;return true;}
    }
    return false;
}
enum { KIND_OTHER,KIND_HEADER,KIND_TABS,KIND_STATUS,KIND_PROGRESS,KIND_BUTTON };
static LRESULT CALLBACK themed_control(HWND h,UINT message,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR kind){
    if(message==WM_NCDESTROY){RemoveWindowSubclass(h,themed_control,id);return DefSubclassProc(h,message,w,l);}
    if(dark && message==WM_NOTIFY){LRESULT result;if(custom_draw((NMHDR *)l,&result))return result;}
    if(dark && message==WM_ERASEBKGND){RECT r;GetClientRect(h,&r);FillRect((HDC)w,&r,win_theme_brush(kind==KIND_OTHER?THEME_SURFACE:THEME_PANEL));return 1;}
    if(dark && (message==WM_PAINT || message==WM_PRINTCLIENT || message==WM_PRINT) && kind!=KIND_OTHER){
        PAINTSTRUCT ps;HDC dc=message==WM_PAINT?BeginPaint(h,&ps):(HDC)w;RECT r;GetClientRect(h,&r);FillRect(dc,&r,win_theme_brush(THEME_PANEL));HFONT font=(HFONT)SendMessageW(h,WM_GETFONT,0,0);HGDIOBJ old=SelectObject(dc,font?font:ui_font?ui_font:GetStockObject(DEFAULT_GUI_FONT));SetTextColor(dc,win_theme_color(THEME_TEXT));
        if(kind==KIND_HEADER){
            int count=Header_GetItemCount(h);for(int i=0;i<count;++i){RECT item;if(!Header_GetItemRect(h,i,&item))continue;wchar_t label[256];HDITEMW info={0};info.mask=HDI_TEXT|HDI_FORMAT;info.pszText=label;info.cchTextMax=256;Header_GetItem(h,i,&info);RECT edge=item;edge.left=edge.right-1;FillRect(dc,&edge,win_theme_brush(THEME_BORDER));item.left+=scale(h,8);item.right-=scale(h,8);text(dc,label,&item,DT_LEFT|DT_END_ELLIPSIS|DT_NOPREFIX);}
        }else if(kind==KIND_TABS){
            int selected=TabCtrl_GetCurSel(h);for(int i=0;i<TabCtrl_GetItemCount(h);++i){RECT item;TabCtrl_GetItemRect(h,i,&item);FillRect(dc,&item,win_theme_brush(i==selected?THEME_SURFACE:THEME_PANEL));wchar_t label[256];TCITEMW info={0};info.mask=TCIF_TEXT;info.pszText=label;info.cchTextMax=256;TabCtrl_GetItem(h,i,&info);text(dc,label,&item,DT_CENTER|DT_NOPREFIX);if(i==selected){RECT line=item;line.top=line.bottom-scale(h,2);FillRect(dc,&line,win_theme_brush(THEME_ACCENT));if(GetFocus()==h){InflateRect(&item,-3,-3);DrawFocusRect(dc,&item);}}}
        }else if(kind==KIND_STATUS){wchar_t label[1024]={0};if(LOWORD(SendMessageW(h,SB_GETTEXTLENGTHW,0,0))<1024)SendMessageW(h,SB_GETTEXTW,0,(LPARAM)label);r.left+=scale(h,5);text(dc,label,&r,DT_LEFT|DT_NOPREFIX);
        }else if(kind==KIND_PROGRESS){PBRANGE range={0};SendMessageW(h,PBM_GETRANGE,0,(LPARAM)&range);int value=(int)SendMessageW(h,PBM_GETPOS,0,0);InflateRect(&r,-1,-1);FillRect(dc,&r,win_theme_brush(THEME_BORDER));if(range.iHigh>range.iLow){r.right=r.left+MulDiv(r.right-r.left,value-range.iLow,range.iHigh-range.iLow);FillRect(dc,&r,win_theme_brush(THEME_ACCENT));}
        }else if(kind==KIND_BUTTON){wchar_t label[256];GetWindowTextW(h,label,256);bool enabled=IsWindowEnabled(h);UINT state=(UINT)SendMessageW(h,BM_GETSTATE,0,0);fill(dc,&r,win_theme_color((state&BST_PUSHED)?THEME_HOVER:THEME_PANEL));FrameRect(dc,&r,win_theme_brush((state&BST_FOCUS)?THEME_ACCENT:THEME_BORDER));SetTextColor(dc,win_theme_color(enabled?THEME_TEXT:THEME_MUTED));text(dc,label,&r,DT_CENTER);}
        SelectObject(dc,old);if(message==WM_PAINT)EndPaint(h,&ps);return 0;
    }
    if(dark && kind==KIND_BUTTON && (message==WM_SETFOCUS || message==WM_KILLFOCUS || message==BM_SETSTATE || message==WM_ENABLE)){LRESULT result=DefSubclassProc(h,message,w,l);InvalidateRect(h,NULL,TRUE);return result;}
    return DefSubclassProc(h,message,w,l);
}
static BOOL CALLBACK style_child(HWND h,LPARAM unused){
    (void)unused;wchar_t cls[64];GetClassNameW(h,cls,64);DWORD_PTR kind=KIND_OTHER;
    if(!wcscmp(cls,WC_HEADERW))kind=KIND_HEADER;else if(!wcscmp(cls,WC_TABCONTROLW))kind=KIND_TABS;else if(!wcscmp(cls,STATUSCLASSNAMEW))kind=KIND_STATUS;else if(!wcscmp(cls,PROGRESS_CLASSW))kind=KIND_PROGRESS;else if(!wcscmp(cls,L"Button") && ((GetWindowLongPtrW(h,GWL_STYLE)&BS_TYPEMASK)==BS_PUSHBUTTON || (GetWindowLongPtrW(h,GWL_STYLE)&BS_TYPEMASK)==BS_DEFPUSHBUTTON))kind=KIND_BUTTON;
    SetWindowSubclass(h,themed_control,1,kind);SetWindowTheme(h,dark?(!wcscmp(cls,L"ComboBox")?L"DarkMode_CFD":L"DarkMode_Explorer"):L"Explorer",NULL);
    if(!wcscmp(cls,WC_LISTVIEWW)){ListView_SetBkColor(h,win_theme_color(THEME_SURFACE));ListView_SetTextBkColor(h,win_theme_color(THEME_SURFACE));ListView_SetTextColor(h,win_theme_color(THEME_TEXT));}
    if(!wcscmp(cls,WC_TREEVIEWW)){TreeView_SetBkColor(h,win_theme_color(THEME_SURFACE));TreeView_SetTextColor(h,win_theme_color(THEME_TEXT));TreeView_SetLineColor(h,win_theme_color(THEME_BORDER));}
    InvalidateRect(h,NULL,TRUE);return TRUE;
}
void win_theme_apply(HWND window,HFONT font){
    if(font)ui_font=font;BOOL enabled=dark;DwmSetWindowAttribute(window,DWMWA_USE_IMMERSIVE_DARK_MODE,&enabled,sizeof(enabled));EnumChildWindows(window,style_child,0);RedrawWindow(window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_FRAME|RDW_ALLCHILDREN);
}
void win_theme_menu_edge(HWND window,HDC target){
    if(!GetMenu(window))return;MENUBARINFO bar={sizeof(bar)};RECT outer;POINT client={0,0};
    if(!GetMenuBarInfo(window,OBJID_MENU,0,&bar) || !GetWindowRect(window,&outer) || !ClientToScreen(window,&client))return;
    RECT edge={bar.rcBar.left-outer.left,bar.rcBar.bottom-outer.top-scale(window,2),bar.rcBar.right-outer.left,client.y-outer.top};
    if(edge.bottom<edge.top)edge.bottom=edge.top+scale(window,2);HDC dc=target?target:GetWindowDC(window);if(dc){FillRect(dc,&edge,win_theme_brush(THEME_PANEL));if(!target)ReleaseDC(window,dc);}
}
typedef struct {MSAAMENUINFO accessible;bool bar,separator,submenu;wchar_t label[256];} menu_item_t;
void win_theme_menu(HMENU menu,bool bar){
    if(!dark)return;MENUINFO background={sizeof(background)};background.fMask=MIM_BACKGROUND;background.hbrBack=win_theme_brush(THEME_PANEL);SetMenuInfo(menu,&background);
    for(int i=0;i<GetMenuItemCount(menu);++i){MENUITEMINFOW item={sizeof(item)};item.fMask=MIIM_FTYPE|MIIM_SUBMENU|MIIM_DATA;GetMenuItemInfoW(menu,(UINT)i,TRUE,&item);if(item.hSubMenu)win_theme_menu(item.hSubMenu,false);if(item.fType&MFT_OWNERDRAW)continue;
        menu_item_t *data=calloc(1,sizeof(*data));if(!data)continue;GetMenuStringW(menu,(UINT)i,data->label,256,MF_BYPOSITION);data->bar=bar;data->separator=(item.fType&MFT_SEPARATOR)!=0;data->submenu=item.hSubMenu!=NULL;data->accessible.dwMSAASignature=MSAA_MENU_SIG;data->accessible.cchWText=(DWORD)wcslen(data->label);data->accessible.pszWText=data->label;
        item.fMask=MIIM_FTYPE|MIIM_DATA;item.fType|=MFT_OWNERDRAW;item.dwItemData=(ULONG_PTR)data;if(!SetMenuItemInfoW(menu,(UINT)i,TRUE,&item))free(data);
    }
}
static void free_menu_data(HMENU menu){for(int i=0;i<GetMenuItemCount(menu);++i){MENUITEMINFOW info={sizeof(info)};info.fMask=MIIM_DATA|MIIM_SUBMENU|MIIM_FTYPE;if(!GetMenuItemInfoW(menu,(UINT)i,TRUE,&info))continue;if(info.hSubMenu)free_menu_data(info.hSubMenu);if((info.fType&MFT_OWNERDRAW) && info.dwItemData){menu_item_t *data=(menu_item_t *)info.dwItemData;if(data->accessible.dwMSAASignature==MSAA_MENU_SIG)free(data);}}}
void win_theme_destroy_menu(HMENU menu){if(menu){free_menu_data(menu);DestroyMenu(menu);}}
bool win_theme_message(HWND window,UINT message,WPARAM w,LPARAM l,LRESULT *result){
    if(GetMenu(window) && (message==WM_NCPAINT || message==WM_NCACTIVATE || (message==WM_PRINT && (l&PRF_NONCLIENT)))){
        int saved=message==WM_PRINT?SaveDC((HDC)w):0;*result=DefWindowProcW(window,message,w,l);if(saved)RestoreDC((HDC)w,saved);win_theme_menu_edge(window,message==WM_PRINT?(HDC)w:NULL);return true;
    }
    if(message==WM_NOTIFY && custom_draw((NMHDR *)l,result))return true;
    if(!dark)return false;
    if(message==WM_INITDIALOG){win_theme_apply(window,NULL);return false;}
    if(message==WM_ERASEBKGND){RECT r;GetClientRect(window,&r);FillRect((HDC)w,&r,win_theme_brush(THEME_PANEL));*result=1;return true;}
    if(message==WM_CTLCOLORSTATIC || message==WM_CTLCOLOREDIT || message==WM_CTLCOLORBTN || message==WM_CTLCOLORLISTBOX){HDC dc=(HDC)w;SetTextColor(dc,win_theme_color(THEME_TEXT));win_theme_color_t color=message==WM_CTLCOLOREDIT || message==WM_CTLCOLORLISTBOX?THEME_SURFACE:THEME_PANEL;SetBkColor(dc,win_theme_color(color));*result=(LRESULT)win_theme_brush(color);return true;}
    if(message==WM_MEASUREITEM && ((MEASUREITEMSTRUCT *)l)->CtlType==ODT_MENU){MEASUREITEMSTRUCT *m=(MEASUREITEMSTRUCT *)l;menu_item_t *item=(menu_item_t *)m->itemData;if(!item || item->accessible.dwMSAASignature!=MSAA_MENU_SIG)return false;HDC dc=GetDC(window);HGDIOBJ old=SelectObject(dc,ui_font?ui_font:GetStockObject(DEFAULT_GUI_FONT));SIZE size;GetTextExtentPoint32W(dc,item->label,(int)wcslen(item->label),&size);SelectObject(dc,old);ReleaseDC(window,dc);m->itemWidth=(UINT)(size.cx+scale(window,item->bar?14:56));m->itemHeight=(UINT)(item->separator?scale(window,8):size.cy+scale(window,10));*result=TRUE;return true;}
    if(message==WM_DRAWITEM && ((DRAWITEMSTRUCT *)l)->CtlType==ODT_MENU){DRAWITEMSTRUCT *d=(DRAWITEMSTRUCT *)l;menu_item_t *item=(menu_item_t *)d->itemData;if(!item || item->accessible.dwMSAASignature!=MSAA_MENU_SIG)return false;bool selected=(d->itemState&ODS_SELECTED)!=0;FillRect(d->hDC,&d->rcItem,win_theme_brush(selected?THEME_HOVER:THEME_PANEL));RECT r=d->rcItem;
        if(item->separator){r.left+=8;r.right-=8;r.top=(r.top+r.bottom)/2;r.bottom=r.top+1;FillRect(d->hDC,&r,win_theme_brush(THEME_BORDER));}
        else{HGDIOBJ old=SelectObject(d->hDC,ui_font?ui_font:GetStockObject(DEFAULT_GUI_FONT));SetTextColor(d->hDC,win_theme_color((d->itemState&(ODS_GRAYED|ODS_DISABLED))?THEME_MUTED:THEME_TEXT));wchar_t label[256];wcscpy_s(label,256,item->label);wchar_t *shortcut=wcschr(label,L'\t');if(shortcut)*shortcut++=0;r.left+=scale(window,item->bar?7:28);r.right-=scale(window,item->bar?7:24);UINT flags=(d->itemState&ODS_NOACCEL)?DT_HIDEPREFIX:0;text(d->hDC,label,&r,DT_LEFT|flags);if(shortcut)text(d->hDC,shortcut,&r,DT_RIGHT|DT_NOPREFIX);if(d->itemState&ODS_CHECKED){RECT mark=d->rcItem;mark.right=mark.left+scale(window,24);text(d->hDC,L"✓",&mark,DT_CENTER);}if(item->submenu && !item->bar){RECT arrow=d->rcItem;arrow.left=arrow.right-scale(window,20);text(d->hDC,L"›",&arrow,DT_CENTER);}SelectObject(d->hDC,old);}
        *result=TRUE;return true;
    }
    if(message==WM_MENUCHAR){HMENU menu=(HMENU)l;wchar_t key=(wchar_t)LOWORD(w);CharUpperBuffW(&key,1);for(int i=0;i<GetMenuItemCount(menu);++i){MENUITEMINFOW info={sizeof(info)};info.fMask=MIIM_DATA|MIIM_FTYPE|MIIM_STATE;GetMenuItemInfoW(menu,(UINT)i,TRUE,&info);if(!(info.fType&MFT_OWNERDRAW) || !info.dwItemData || (info.fState&(MFS_DISABLED|MFS_GRAYED)))continue;menu_item_t *item=(menu_item_t *)info.dwItemData;for(const wchar_t *s=item->label;*s;++s)if(*s==L'&' && s[1] && s[1]!=L'&'){wchar_t c=s[1];CharUpperBuffW(&c,1);if(c==key){*result=MAKELRESULT(i,MNC_EXECUTE);return true;}}}}
    return false;
}
void win_theme_cleanup(void){for(unsigned i=0;i<7;++i)if(brushes[i]){DeleteObject(brushes[i]);brushes[i]=NULL;}ui_font=NULL;}
