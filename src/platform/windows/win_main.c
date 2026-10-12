#include "win_net.h"
#include "build_info.h"
#include "ports.h"
#include "vendor.h"
#include "monitor.h"
#include "profile.h"
#include "statistics.h"
#include "win_files.h"
#include "win_profile_ui.h"
#include "details.h"
#include "win_tools.h"
#include "win_toolbar_icons.h"
#include "../../resources/resource.h"
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <wctype.h>
#define ID_SCAN 101
#define ID_STOP 102
#define ID_REFRESH 103
#define ID_EN 104
#define ID_RU 105
#define ID_ABOUT 106
#define ID_EXIT 107
#define ID_SINGLE 108
#define ID_PORTS 109
#define ID_CUSTOM 110
#define ID_FILTER 111
#define ID_WEB 112
#define ID_HTTPS 113
#define ID_RDP 114
#define ID_SMB 115
#define ID_COPY_IP 116
#define ID_COPY_NAME 117
#define ID_COPY_MAC 118
#define ID_PROPERTIES 119
#define ID_MONITOR 120
#define ID_RESCAN 121
#define ID_ACKNOWLEDGE 122
#define ID_INTERVAL_FIRST 130
#define ID_PROFILE_OPEN 140
#define ID_PROFILE_SAVE 141
#define ID_PROFILE_EDIT 142
#define ID_PROFILE_NEW 143
#define ID_EXPORT_CSV 150
#define ID_EXPORT_TXT 151
#define ID_EXPORT_HTML 152
#define ID_EXPORT_MENU 153
#define ID_STATISTICS 154
#define ID_PING 155
#define ID_TRACE 156
#define ID_SSH 157
#define ID_NOTES 158
#define ID_SYSTEM_LANGUAGE 159
#define HOST_CAPACITY 65536
#define LOG_CAPACITY 10000
typedef struct {
    HWND window, toolbar, tree, list, tabs, detail, status, summary, progress, filter, log_view, statistics_view;
    HFONT font;
    HIMAGELIST images;
    win_backend_t backend;
    win_interface_t interfaces[MAX_INTERFACES];
    size_t interface_count;
    int selected, split, bottom, drag, sort_column, sort_direction;
    UINT dpi;
    bool russian, smoke, smoke_close_scan, smoke_close_tools, smoke_dpi, preview, closing, scanning;
    bool system_language;
    LONG generation;
    ULONGLONG started;
    ULONGLONG elapsed;
    lan_host_t hosts[HOST_CAPACITY];
    size_t host_count;
    wchar_t ini[MAX_PATH];
    wchar_t filter_text[128];
    vendor_table_t vendors;
    monitor_t model;
    monitor_event_t log[LOG_CAPACITY];
    uint64_t displayed_log_sequence;
    bool monitoring, has_monitor_range, round_healthy, dialog_open;
    unsigned monitor_interval;
    ipv4_range_t monitor_range;
    ULONGLONG next_monitor;
    profile_t profile;
    bool custom_profile, loading_tree;
    wchar_t profile_path[MAX_PATH];
    saved_profile_t saved_profiles[32];
    size_t saved_count;
    HTREEITEM interface_items[MAX_INTERFACES], profile_root;
    win_export_job_t *export_job;
    details_store_t details;
    win_tools_job_t *tools_job;
    uint32_t session, tools_session, trace_target, trace_interface;
    bool tools_trace;
    ping_sample_t trace[30]; size_t trace_count;
} app_t;
static app_t app;
static void load_profile_path(const wchar_t *path);
static void save_settings(void);
static bool language_is_russian(const wchar_t *preference, LANGID system) {
    if (!wcscmp(preference,L"ru")) return true;
    if (!wcscmp(preference,L"en")) return false;
    return PRIMARYLANGID(system) == LANG_RUSSIAN;
}
static const wchar_t *columns_en[] = {L"Status",L"IP Address",L"Host Name",L"MAC Address",L"Vendor",L"Device / OS",L"Latency",L"Services",L"First Seen",L"Last Seen"};
static const wchar_t *columns_ru[] = {L"Статус",L"IP-адрес",L"Имя узла",L"MAC-адрес",L"Производитель",L"Устройство / ОС",L"Задержка",L"Сервисы",L"Первое обнаружение",L"Последний ответ"};
static const int widths[] = {100,125,160,155,125,130,85,110,150,150};
static const unsigned intervals[] = {5,10,30,60,300};
static const wchar_t *tr(const wchar_t *en, const wchar_t *ru) { return app.russian ? ru : en; }
static int px(int logical) { return MulDiv(logical, (int)app.dpi, 96); }
static void ip_text(uint32_t ip, wchar_t *out, size_t cap) { swprintf_s(out, cap, L"%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255); }
static void utf8_text(const char *input, wchar_t *output, int capacity) {
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, output, capacity)) output[0] = 0;
}
static void timestamp(uint64_t seconds, wchar_t *out, size_t cap) {
    if (!seconds) { swprintf_s(out, cap, L"—"); return; }
    __time64_t value = (__time64_t)seconds;
    struct tm local;
    if (_localtime64_s(&local, &value)) { out[0] = 0; return; }
    wcsftime(out, cap, L"%Y-%m-%d %H:%M:%S", &local);
}
static void set_font(void) {
    NONCLIENTMETRICSW metrics = {0}; metrics.cbSize = sizeof(metrics);
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, app.dpi);
    HFONT old = app.font; app.font = CreateFontIndirectW(&metrics.lfMessageFont);
    HWND controls[] = {app.toolbar,app.tree,app.list,app.tabs,app.detail,app.status,app.summary,app.filter,app.log_view,app.statistics_view};
    for (unsigned i = 0; i < sizeof(controls)/sizeof(controls[0]); ++i) SendMessageW(controls[i], WM_SETFONT, (WPARAM)app.font, TRUE);
    if (old) DeleteObject(old);
}
static void layout(void) {
    RECT r; GetClientRect(app.window, &r);
    int w = r.right, h = r.bottom;
    SendMessageW(app.status, WM_SIZE, 0, 0);
    RECT sr; GetWindowRect(app.status, &sr);
    int sh = sr.bottom - sr.top, th = px(30), title = px(28), gap = px(5);
    SIZE toolbar_size = {0}; SendMessageW(app.toolbar,TB_GETMAXSIZE,0,(LPARAM)&toolbar_size);
    if (toolbar_size.cy+px(4) > th) th = toolbar_size.cy+px(4);
    bool filter_below = toolbar_size.cx + px(140) > w;
    if (filter_below) th += px(30);
    int left = px(app.split), details = px(app.bottom);
    if (left > w - px(300)) left = w - px(300);
    if (left < px(130)) left = px(130);
    int top = th + title, bh = h - sh - top;
    if (details > bh - px(120)) details = bh - px(120);
    if (details < px(75)) details = px(75);
    MoveWindow(app.toolbar, 0, 0, w, th, TRUE);
    int filter_width = filter_below ? w-px(20) : w-toolbar_size.cx-px(20); if (filter_width > px(200) && !filter_below) filter_width = px(200); if (filter_width < px(100)) filter_width = px(100);
    MoveWindow(app.filter,w-filter_width-px(10),filter_below ? th-px(28) : px(4),filter_width,px(24),TRUE);
    MoveWindow(app.summary, px(8), th + px(3), w - px(16), title - px(7), TRUE);
    MoveWindow(app.tree, 0, top, left, bh, TRUE);
    MoveWindow(app.list, left + gap, top, w - left - gap, bh - details - gap, TRUE);
    MoveWindow(app.tabs, left + gap, h - sh - details, w - left - gap, details, TRUE);
    RECT tab; GetClientRect(app.tabs, &tab); TabCtrl_AdjustRect(app.tabs, FALSE, &tab);
    MoveWindow(app.detail, left + gap + tab.left + px(4), h - sh - details + tab.top + px(4), tab.right - tab.left - px(8), tab.bottom - tab.top - px(8), TRUE);
    MoveWindow(app.log_view, left + gap + tab.left + px(4), h - sh - details + tab.top + px(4), tab.right - tab.left - px(8), tab.bottom - tab.top - px(8), TRUE);
    MoveWindow(app.statistics_view, left + gap + tab.left + px(4), h - sh - details + tab.top + px(4), tab.right - tab.left - px(8), tab.bottom - tab.top - px(8), TRUE);
    int parts[] = {w - px(195), -1}; SendMessageW(app.status, SB_SETPARTS, 2, (LPARAM)parts);
    MoveWindow(app.progress, w - px(185), h - sh + px(5), px(175), sh - px(10), TRUE);
    InvalidateRect(app.window, NULL, FALSE);
}
static void update_summary(void) {
    wchar_t text[768], ip[32], gw[32];
    if (app.selected < 0) swprintf_s(text, 768, L"%ls", tr(L"Select a network interface to begin.",L"Выберите сетевой интерфейс для сканирования."));
    else {
        win_interface_t *v = &app.interfaces[app.selected];
        ip_text(v->ip, ip, 32); ip_text(v->gateway, gw, 32);
        wchar_t name[256]; utf8_text(app.profile.name,name,256);
        swprintf_s(text, 768, L"%ls  |  %ls/%u  |  %ls: %ls  |  %ls: %u  |  %ls: %ls (%zu)", v->name, ip, v->prefix, tr(L"Gateway", L"Шлюз"), gw, tr(L"Metric",L"Метрика"), v->metric,tr(L"Profile",L"Профиль"),name,app.profile.range_count);
    }
    SetWindowTextW(app.summary, text);
}
static void update_status(void) {
    unsigned online = 0, offline = 0, fresh = 0;
    for (size_t i = 0; i < app.host_count; ++i) { if (app.hosts[i].online) ++online; else if (app.hosts[i].first_seen) ++offline; if (app.hosts[i].new_device) ++fresh; }
    wchar_t text[256];
    double elapsed = app.scanning ? (double)(GetTickCount64() - app.started)/1000 : (double)app.elapsed/1000;
    swprintf_s(text, 256, L"%ls  |  %ls: %u  |  %ls: %u  |  %ls: %u  |  %ld/%u  |  %.1f s",
        app.closing ? tr(L"Closing…",L"Завершение…") : app.scanning ? (app.backend.cancel ? tr(L"Stopping…",L"Остановка…") : tr(L"Scanning",L"Сканирование")) : app.tools_job ? (app.tools_trace ? tr(L"Tracing…",L"Трассировка…") : L"Ping…") : app.export_job ? tr(L"Exporting…",L"Экспорт…") : tr(L"Ready",L"Готово"),
        tr(L"Online",L"В сети"), online, tr(L"Offline",L"Не в сети"), offline, tr(L"New",L"Новые"), fresh, app.generation ? app.backend.completed : 0, app.generation ? app.backend.total : 0, elapsed);
    SendMessageW(app.status, SB_SETTEXTW, 0, (LPARAM)text);
    SendMessageW(app.progress, PBM_SETRANGE32, 0, app.generation && app.backend.total ? app.backend.total : 1);
    SendMessageW(app.progress, PBM_SETPOS, app.generation ? app.backend.completed : 0, 0);
    SendMessageW(app.toolbar, TB_ENABLEBUTTON, ID_SCAN, MAKELONG(!app.scanning && !app.tools_job && app.selected >= 0 && !app.closing,0));
    SendMessageW(app.toolbar, TB_ENABLEBUTTON, ID_STOP, MAKELONG((app.scanning && !app.backend.cancel) || app.monitoring || app.tools_job,0));
    SendMessageW(app.toolbar, TB_ENABLEBUTTON, ID_REFRESH, MAKELONG(!app.scanning && !app.closing && !app.monitoring,0));
    SendMessageW(app.toolbar, TB_ENABLEBUTTON, ID_SINGLE, MAKELONG(!app.scanning && app.selected >= 0 && !app.closing,0));
    SendMessageW(app.toolbar, TB_ENABLEBUTTON, ID_EXPORT_MENU, MAKELONG(!app.export_job && !app.closing,0));
    SendMessageW(app.toolbar, TB_ENABLEBUTTON, ID_MONITOR, MAKELONG(app.selected >= 0 && !app.closing,0));
    SendMessageW(app.toolbar, TB_CHECKBUTTON, ID_MONITOR, MAKELONG(app.monitoring,0));
    CheckMenuItem(GetMenu(app.window),ID_MONITOR,MF_BYCOMMAND | (app.monitoring ? MF_CHECKED : MF_UNCHECKED));
    if (app.monitoring && app.selected >= 0) {
        wchar_t summary[1024], suffix[128];
        update_summary(); GetWindowTextW(app.summary,summary,1024);
        swprintf_s(suffix,128,L"  |  %ls: %u s",tr(L"Monitoring",L"Мониторинг"),app.monitor_interval);
        wcscat_s(summary,1024,suffix); SetWindowTextW(app.summary,summary);
    }
    EnableWindow(app.tree,!app.closing);
}
static void row_text(const lan_host_t *h, int col, wchar_t *s, size_t cap) {
    s[0] = 0;
    switch (col) {
    case 0: swprintf_s(s,cap,L"%ls", !h->first_seen ? tr(L"UNKNOWN",L"НЕИЗВЕСТНО") : !h->online ? tr(L"OFFLINE",L"НЕ В СЕТИ") : h->new_device ? tr(L"NEW",L"НОВОЕ") : h->changed ? tr(L"CHANGED",L"ИЗМЕНЕНО") : tr(L"ONLINE",L"В СЕТИ")); break;
    case 1: ip_text(ipv4_value(&h->address),s,cap); break;
    case 2: if (h->hostname[0]) utf8_text(h->hostname,s,(int)cap); else swprintf_s(s,cap,L"%ls",tr(L"<unknown>",L"<неизвестно>")); break;
    case 3: if (h->has_mac) swprintf_s(s,cap,L"%02X:%02X:%02X:%02X:%02X:%02X",h->mac[0],h->mac[1],h->mac[2],h->mac[3],h->mac[4],h->mac[5]); break;
    case 4: {
        const char *vendor = h->has_mac ? vendor_lookup(&app.vendors,h->mac) : NULL;
        if (vendor) utf8_text(vendor,s,(int)cap);
        else swprintf_s(s,cap,L"%ls",h->has_mac && (h->mac[0]&2) ? tr(L"Local / private MAC",L"Локальный / приватный MAC") : tr(L"Unknown",L"Неизвестно"));
        break;
    }
    case 5:
        if (h->os_family == OS_WINDOWS) swprintf_s(s,cap,L"Windows — %ls (%u%%)",tr(L"probable",L"вероятно"),h->os_confidence);
        else if (h->os_family == OS_LINUX) swprintf_s(s,cap,L"Linux/Unix — %ls (%u%%)",tr(L"probable",L"вероятно"),h->os_confidence);
        else if (h->os_family == OS_PRINTER) swprintf_s(s,cap,L"%ls (%u%%)",tr(L"Printer — probable",L"Принтер — вероятно"),h->os_confidence);
        else swprintf_s(s,cap,L"%ls",tr(L"Unknown",L"Неизвестно"));
        break;
    case 7: {
        size_t used = 0;
        for (unsigned i = 0; i < h->port_count; ++i) {
            wchar_t service[48]; utf8_text(port_service(h->open_ports[i]),service,48);
            wchar_t entry[80]; swprintf_s(entry,80,L"%ls%ls:%u",used ? L", " : L"",service,h->open_ports[i]);
            size_t n = wcslen(entry); if (used+n+1 >= cap) break;
            memcpy(s+used,entry,(n+1)*sizeof(wchar_t)); used += n;
        }
        if (!used) swprintf_s(s,cap,L"%ls",h->ports_checked ? tr(L"None detected",L"Не обнаружены") : L"—");
        break;
    }
    case 6: if (h->has_latency) swprintf_s(s,cap,L"%u ms",h->latency_ms); break;
    case 8: timestamp(h->first_seen,s,cap); break;
    case 9: timestamp(h->last_seen,s,cap); break;
    }
}
static int row_for(size_t index) {
    LVFINDINFOW find = {0}; find.flags = LVFI_PARAM; find.lParam = (LPARAM)index;
    return ListView_FindItem(app.list, -1, &find);
}
static bool matches_filter(const lan_host_t *host) {
    if (!app.filter_text[0]) return true;
    for (int col = 0; col < 8; ++col) {
        wchar_t text[1024]; row_text(host,col,text,1024);
        for (size_t i = 0; text[i]; ++i) text[i] = (wchar_t)towlower(text[i]);
        if (wcsstr(text,app.filter_text)) return true;
    }
    return false;
}
static void update_row(size_t index) {
    int row = row_for(index);
    if (!matches_filter(&app.hosts[index])) { if (row >= 0) ListView_DeleteItem(app.list,row); return; }
    LVITEMW item = {0}; item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE; item.iItem = row < 0 ? ListView_GetItemCount(app.list) : row;
    item.lParam = (LPARAM)index; item.iImage = app.hosts[index].online ? 0 : 1;
    wchar_t text[512]; row_text(&app.hosts[index],0,text,512); item.pszText = text;
    if (row < 0) row = ListView_InsertItem(app.list,&item); else ListView_SetItem(app.list,&item);
    for (int c = 1; c < 10; ++c) { row_text(&app.hosts[index],c,text,512); ListView_SetItemText(app.list,row,c,text); }
}
static size_t selected_host(void) {
    int row = ListView_GetNextItem(app.list,-1,LVNI_SELECTED);
    if (row < 0) return SIZE_MAX;
    LVITEMW item = {0}; item.mask = LVIF_PARAM; item.iItem = row;
    if (!ListView_GetItem(app.list,&item)) return SIZE_MAX;
    return (size_t)item.lParam;
}
static const wchar_t *event_name(monitor_event_type_t type) {
    static const wchar_t *en[] = {L"Scan started",L"Scan finished",L"Scan cancelled",L"HOST ONLINE",L"HOST OFFLINE",L"NEW DEVICE",L"IP CHANGED",L"MAC CHANGED",L"HOSTNAME CHANGED",L"Scan error",L"Export started",L"Export finished",L"Export error"};
    static const wchar_t *ru[] = {L"Сканирование начато",L"Сканирование завершено",L"Сканирование отменено",L"УЗЕЛ В СЕТИ",L"УЗЕЛ НЕ В СЕТИ",L"НОВОЕ УСТРОЙСТВО",L"IP ИЗМЕНЁН",L"MAC ИЗМЕНЁН",L"ИМЯ ИЗМЕНЕНО",L"Ошибка сканирования",L"Экспорт начат",L"Экспорт завершён",L"Ошибка экспорта"};
    return type <= MON_EXPORT_ERROR ? (app.russian ? ru[type] : en[type]) : L"—";
}
static void event_detail(const monitor_event_t *event, wchar_t *text, size_t capacity) {
    wchar_t previous[256], current[256]; utf8_text(event->previous,previous,256); utf8_text(event->current,current,256);
    if (previous[0]) swprintf_s(text,capacity,L"%ls → %ls",previous,current);
    else swprintf_s(text,capacity,L"%ls",current);
}
static void sync_log(bool rebuild) {
    const monitor_event_t *oldest = monitor_log_at(&app.model,0);
    if (rebuild || (oldest && app.displayed_log_sequence+1 < oldest->sequence)) { ListView_DeleteAllItems(app.log_view); app.displayed_log_sequence = 0; }
    SendMessageW(app.log_view,WM_SETREDRAW,FALSE,0);
    for (size_t i = 0; i < app.model.log_count; ++i) {
        const monitor_event_t *event = monitor_log_at(&app.model,i);
        if (event->sequence <= app.displayed_log_sequence) continue;
        if (ListView_GetItemCount(app.log_view) >= LOG_CAPACITY) ListView_DeleteItem(app.log_view,0);
        wchar_t when[64], ip[32] = L"", details[768]; timestamp(event->timestamp,when,64);
        if (event->address.family == 4) ip_text(ipv4_value(&event->address),ip,32);
        event_detail(event,details,768);
        LVITEMW item = {0}; item.mask = LVIF_TEXT; item.iItem = ListView_GetItemCount(app.log_view); item.pszText = when;
        int row = ListView_InsertItem(app.log_view,&item);
        ListView_SetItemText(app.log_view,row,1,(wchar_t *)event_name(event->type)); ListView_SetItemText(app.log_view,row,2,ip); ListView_SetItemText(app.log_view,row,3,details);
        app.displayed_log_sequence = event->sequence;
    }
    SendMessageW(app.log_view,WM_SETREDRAW,TRUE,0); InvalidateRect(app.log_view,NULL,FALSE);
}
static void show_detail(void) {
    int tab = TabCtrl_GetCurSel(app.tabs);
    ShowWindow(app.log_view,tab == 4 ? SW_SHOW : SW_HIDE); ShowWindow(app.detail,tab < 4 || tab == 6 ? SW_SHOW : SW_HIDE); ShowWindow(app.statistics_view,tab == 5 ? SW_SHOW : SW_HIDE);
    if (tab == 5) { InvalidateRect(app.statistics_view,NULL,TRUE); return; }
    if (tab == 4) { sync_log(false); return; }
    size_t index = selected_host();
    if (index == SIZE_MAX) { SetWindowTextW(app.detail,tr(L"Select a host to view details. Double-click a row or press Enter.",L"Выберите узел для просмотра. Двойной щелчок по строке или Enter.")); return; }
    lan_host_t *h = &app.hosts[index];
    wchar_t text[4096], ip[32], name[256], mac[48], first[64], last[64];
    row_text(h,1,ip,32); row_text(h,2,name,256); row_text(h,3,mac,48); timestamp(h->first_seen,first,64); timestamp(h->last_seen,last,64);
    if (tab == 3) {
        text[0] = 0;
        for (size_t i = app.model.log_count; i > 0; --i) {
            const monitor_event_t *event = monitor_log_at(&app.model,i-1); if (event->host_index != index) continue;
            wchar_t when[64], details[768], line[1024]; timestamp(event->timestamp,when,64); event_detail(event,details,768);
            swprintf_s(line,1024,L"%ls  %ls  %ls\r\n",when,event_name(event->type),details);
            if (wcslen(text)+wcslen(line)+1 >= 4096) break;
            wcscat_s(text,4096,line);
        }
        if (!text[0]) swprintf_s(text,4096,L"%ls",tr(L"No events for this host in the current session.",L"Для узла нет событий в текущей сессии."));
    } else if (tab == 2) {
        swprintf_s(text,4096,L"Ping: %ls\r\n",ip); unsigned displayed = 0;
        for (size_t i = app.details.ping_count; i && displayed < 60; --i) {
            const ping_sample_t *sample = details_ping_at(&app.details,i-1); if (sample->ip != ipv4_value(&h->address) || sample->interface_index != h->interface_index) continue;
            wchar_t when[64], line[160]; timestamp(sample->timestamp,when,64);
            if (sample->received && !sample->status) swprintf_s(line,160,L"%ls  %u ms\r\n",when,sample->latency_ms);
            else swprintf_s(line,160,L"%ls  %ls (%u)\r\n",when,tr(L"No echo reply",L"Нет ответа Echo"),sample->status);
            if (wcslen(text)+wcslen(line)+1 >= 4096) break; wcscat_s(text,4096,line); ++displayed;
        }
        if (!displayed) wcscat_s(text,4096,tr(L"No ping samples. Use Ping from the context menu.",L"Истории ping нет. Выберите Ping в контекстном меню."));
    } else if (tab == 6) {
        swprintf_s(text,4096,L"%ls: %ls\r\n",tr(L"Trace",L"Трассировка"),ip);
        if (app.trace_target == ipv4_value(&h->address) && app.trace_interface == h->interface_index) {
            for (size_t i = 0; i < app.trace_count; ++i) { wchar_t hop_ip[32], line[160]; ping_sample_t *sample = &app.trace[i];
                if (sample->received) { ip_text(sample->reply_ip,hop_ip,32); swprintf_s(line,160,L"%u  %ls  %u ms  (%u)\r\n",sample->hop,hop_ip,sample->latency_ms,sample->status); }
                else swprintf_s(line,160,L"%u  *  (%u)\r\n",sample->hop,sample->status);
                wcscat_s(text,4096,line);
            }
        }
        if (app.tools_job && app.tools_trace && app.trace_target == ipv4_value(&h->address) && app.trace_interface == h->interface_index) wcscat_s(text,4096,tr(L"Tracing… Stop cancels.",L"Трассировка… Стоп отменяет."));
        else if (!app.trace_count || app.trace_target != ipv4_value(&h->address)) wcscat_s(text,4096,tr(L"Use Traceroute from the context menu.",L"Выберите Трассировку в контекстном меню."));
    } else if (TabCtrl_GetCurSel(app.tabs) == 1) {
        swprintf_s(text,4096,L"%ls: %ls\r\n%ls\r\n",tr(L"Host",L"Узел"),ip,tr(L"Detected open TCP ports (service names are port-based hints):",L"Обнаруженные открытые TCP-порты (названия сервисов — подсказки по номеру порта):"));
        for (unsigned i = 0; i < h->port_count; ++i) {
            wchar_t service[48], line[96]; utf8_text(port_service(h->open_ports[i]),service,48);
            swprintf_s(line,96,L"%u  %ls\r\n",h->open_ports[i],service); wcscat_s(text,4096,line);
        }
        if (!h->ports_checked) wcscat_s(text,4096,tr(L"Scan not completed.",L"Проверка не завершена."));
        else if (!h->port_count) wcscat_s(text,4096,tr(L"No open ports in the tested set.",L"В проверенном наборе открытых портов нет."));
        if (h->ports_truncated) wcscat_s(text,4096,tr(L"\r\nOnly the first 64 open ports are shown.",L"\r\nПоказаны только первые 64 открытых порта."));
    } else {
        wchar_t vendor[512],device[256],status[64]; row_text(h,4,vendor,512); row_text(h,5,device,256); row_text(h,0,status,64);
        swprintf_s(text,4096,L"IP: %ls    %ls: %ls\r\nMAC: %ls    %ls: %ls\r\n%ls: %ls\r\n%ls: %ls    %ls: %ls\r\n%ls: %ls    %ls: %ls",
        ip,tr(L"Host name",L"Имя узла"),name,mac,tr(L"Status",L"Статус"),status,
        tr(L"Interface",L"Интерфейс"), app.selected >= 0 ? app.interfaces[app.selected].name : L"—",
        tr(L"First seen",L"Первое обнаружение"),first,tr(L"Last seen",L"Последний ответ"),last,
        tr(L"Vendor",L"Производитель"),vendor,tr(L"Device / OS",L"Устройство / ОС"),device);
        wchar_t notes[512]; utf8_text(details_note(&app.details,ipv4_value(&h->address),h->interface_index),notes,512);
        wcscat_s(text,4096,tr(L"\r\nNotes (session): ",L"\r\nЗаметки (сессия): ")); wcscat_s(text,4096,notes);
    }
    SetWindowTextW(app.detail,text);
}
static int CALLBACK compare_rows(LPARAM a, LPARAM b, LPARAM unused) {
    (void)unused;
    lan_host_t *x = &app.hosts[a], *y = &app.hosts[b]; int result;
    if (app.sort_column == 1) { uint32_t p = ipv4_value(&x->address), q = ipv4_value(&y->address); result = (p > q) - (p < q); }
    else if (app.sort_column == 6) result = (x->latency_ms > y->latency_ms) - (x->latency_ms < y->latency_ms);
    else { wchar_t p[512], q[512]; row_text(x,app.sort_column,p,512); row_text(y,app.sort_column,q,512); result = _wcsicmp(p,q); }
    return result * app.sort_direction;
}
static void create_menu(void) {
    HMENU old = GetMenu(app.window), menu = CreateMenu(), file = CreatePopupMenu(), scan = CreatePopupMenu(), view = CreatePopupMenu(), language = CreatePopupMenu(), help = CreatePopupMenu(), interval = CreatePopupMenu(), exports = CreatePopupMenu();
    AppendMenuW(file,MF_STRING,ID_PROFILE_NEW,tr(L"New &profile",L"&Новый профиль"));
    AppendMenuW(file,MF_STRING,ID_PROFILE_OPEN,tr(L"&Open profile…\tCtrl+O",L"&Открыть профиль…\tCtrl+O"));
    AppendMenuW(file,MF_STRING,ID_PROFILE_EDIT,tr(L"&Edit profile / ranges…",L"&Изменить профиль / диапазоны…"));
    AppendMenuW(file,MF_STRING,ID_PROFILE_SAVE,tr(L"&Save profile…\tCtrl+S",L"&Сохранить профиль…\tCtrl+S"));
    AppendMenuW(file,MF_SEPARATOR,0,NULL);
    AppendMenuW(exports,MF_STRING,ID_EXPORT_CSV,L"CSV"); AppendMenuW(exports,MF_STRING,ID_EXPORT_TXT,L"TXT"); AppendMenuW(exports,MF_STRING,ID_EXPORT_HTML,L"HTML");
    AppendMenuW(file,MF_POPUP,(UINT_PTR)exports,tr(L"&Export all hosts",L"&Экспорт всех узлов"));
    AppendMenuW(file,MF_SEPARATOR,0,NULL);
    AppendMenuW(file,MF_STRING,ID_EXIT,tr(L"E&xit",L"Вы&ход"));
    AppendMenuW(scan,MF_STRING,ID_SCAN,tr(L"&Scan\tF5",L"&Сканировать\tF5"));
    AppendMenuW(scan,MF_STRING,ID_STOP,tr(L"S&top\tEsc",L"&Остановить\tEsc"));
    AppendMenuW(scan,MF_SEPARATOR,0,NULL);
    AppendMenuW(scan,MF_STRING,ID_SINGLE,tr(L"Single &host…",L"&Отдельный узел…"));
    AppendMenuW(scan,MF_STRING,ID_PORTS,tr(L"Scan &ports",L"Сканировать &порты"));
    AppendMenuW(scan,MF_STRING,ID_CUSTOM,tr(L"&Custom ports…",L"Произвольные пор&ты…"));
    AppendMenuW(scan,MF_STRING,ID_PING,L"&Ping"); AppendMenuW(scan,MF_STRING,ID_TRACE,tr(L"&Traceroute",L"&Трассировка"));
    AppendMenuW(scan,MF_SEPARATOR,0,NULL);
    AppendMenuW(scan,MF_STRING | (app.monitoring ? MF_CHECKED : 0),ID_MONITOR,tr(L"&Monitoring",L"&Мониторинг"));
    AppendMenuW(scan,MF_STRING,ID_RESCAN,tr(L"&Rescan host",L"Повторно проверить &узел"));
    AppendMenuW(view,MF_STRING,ID_REFRESH,tr(L"Refresh &interfaces",L"&Обновить интерфейсы"));
    AppendMenuW(view,MF_STRING,ID_STATISTICS,tr(L"&Statistics",L"&Статистика"));
    AppendMenuW(language,MF_STRING | (app.system_language ? MF_CHECKED : 0),ID_SYSTEM_LANGUAGE,tr(L"System language (default)",L"Язык системы (по умолчанию)"));
    AppendMenuW(language,MF_SEPARATOR,0,NULL);
    AppendMenuW(language,MF_STRING | (!app.system_language && !app.russian ? MF_CHECKED : 0),ID_EN,L"English");
    AppendMenuW(language,MF_STRING | (!app.system_language && app.russian ? MF_CHECKED : 0),ID_RU,L"Русский");
    AppendMenuW(view,MF_POPUP,(UINT_PTR)language,tr(L"&Language",L"&Язык"));
    for (unsigned i = 0; i < 5; ++i) { wchar_t label[64]; swprintf_s(label,64,L"%u %ls",intervals[i],tr(L"seconds",L"секунд")); AppendMenuW(interval,MF_STRING | (app.monitor_interval == intervals[i] ? MF_CHECKED : 0),ID_INTERVAL_FIRST+i,label); }
    AppendMenuW(view,MF_POPUP,(UINT_PTR)interval,tr(L"Monitoring &interval",L"&Интервал мониторинга"));
    AppendMenuW(help,MF_STRING,ID_ABOUT,tr(L"&About",L"&О программе"));
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)file,tr(L"&File",L"&Файл"));
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)scan,tr(L"&Scan",L"&Сканирование"));
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)view,tr(L"&View",L"&Вид"));
    AppendMenuW(menu,MF_POPUP,(UINT_PTR)help,tr(L"&Help",L"&Справка"));
    SetMenu(app.window,menu); if (old) DestroyMenu(old); DrawMenuBar(app.window);
}
static void localize(void) {
    create_menu();
    const wchar_t *buttons[] = {tr(L"Scan",L"Сканировать"),tr(L"Stop",L"Стоп"),tr(L"Refresh",L"Обновить"),tr(L"Host",L"Узел"),tr(L"Monitoring",L"Мониторинг"),tr(L"Export",L"Экспорт")};
    const int ids[] = {ID_SCAN,ID_STOP,ID_REFRESH,ID_SINGLE,ID_MONITOR,ID_EXPORT_MENU};
    HDC toolbar_dc = GetDC(app.toolbar); HGDIOBJ previous_font = SelectObject(toolbar_dc,app.font);
    TEXTMETRICW metrics; GetTextMetricsW(toolbar_dc,&metrics); int height = metrics.tmHeight > px(20) ? metrics.tmHeight : px(20); SendMessageW(app.toolbar,TB_SETBUTTONSIZE,0,MAKELPARAM(1,height+px(10)));
    for (int i = 0; i < 6; ++i) { SIZE extent = {0}; GetTextExtentPoint32W(toolbar_dc,buttons[i],(int)wcslen(buttons[i]),&extent); TBBUTTONINFOW b = {0}; b.cbSize = sizeof(b); b.dwMask = TBIF_TEXT|TBIF_SIZE|TBIF_STYLE; b.fsStyle = BTNS_SHOWTEXT|(i == 4 ? BTNS_CHECK : i == 5 ? BTNS_DROPDOWN : BTNS_BUTTON); b.cx = (WORD)(extent.cx+px(i == 5 ? 64 : 48)); b.pszText = (wchar_t *)buttons[i]; SendMessageW(app.toolbar,TB_SETBUTTONINFOW,ids[i],(LPARAM)&b); }
    SelectObject(toolbar_dc,previous_font); ReleaseDC(app.toolbar,toolbar_dc);
    SendMessageW(app.filter,EM_SETCUEBANNER,TRUE,(LPARAM)tr(L"Filter hosts…",L"Фильтр устройств…"));
    for (int i = 0; i < 10; ++i) { LVCOLUMNW c = {0}; c.mask = LVCF_TEXT; c.pszText = (wchar_t *)(app.russian ? columns_ru[i] : columns_en[i]); ListView_SetColumn(app.list,i,&c); }
    const wchar_t *tabs[] = {tr(L"General",L"Общие"),tr(L"Ports",L"Порты"),L"Ping",tr(L"History",L"История"),tr(L"Log",L"Журнал"),tr(L"Statistics",L"Статистика"),tr(L"Trace",L"Трассировка")};
    for (int i = 0; i < 7; ++i) { TCITEMW item = {0}; item.mask = TCIF_TEXT; item.pszText = (wchar_t *)tabs[i]; TabCtrl_SetItem(app.tabs,i,&item); }
    const wchar_t *log_columns[] = {tr(L"Time",L"Время"),tr(L"Event",L"Событие"),L"IP",tr(L"Details",L"Подробности")};
    for (int i = 0; i < 4; ++i) { LVCOLUMNW c = {0}; c.mask = LVCF_TEXT; c.pszText = (wchar_t *)log_columns[i]; ListView_SetColumn(app.log_view,i,&c); }
    sync_log(true);
    if (app.profile_root) { TVITEMW item = {0}; item.mask = TVIF_TEXT; item.hItem = app.profile_root; item.pszText = (wchar_t *)tr(L"Saved profiles",L"Сохранённые профили"); TreeView_SetItem(app.tree,&item); }
    SendMessageW(app.toolbar,TB_AUTOSIZE,0,0);
    for (size_t i = 0; i < app.host_count; ++i) update_row(i);
    update_summary(); update_status(); show_detail(); layout();
}
static void reset_session(void) {
    ++app.session; win_tools_cancel(app.tools_job); details_reset(&app.details); app.trace_count = 0; app.trace_target = 0;
    app.generation = 0; app.has_monitor_range = false;
    app.started = app.elapsed = 0;
    monitor_init(&app.model,app.hosts,&app.host_count,HOST_CAPACITY,app.log,LOG_CAPACITY);
    app.displayed_log_sequence = 0; ListView_DeleteAllItems(app.list); ListView_DeleteAllItems(app.log_view);
}
static void bind_profile_interface(void) {
    if (app.selected < 0 || (size_t)app.selected >= app.interface_count) return;
    win_interface_t *v = &app.interfaces[app.selected];
    snprintf(app.profile.interface_id,sizeof(app.profile.interface_id),"%s",v->id);
    if (!WideCharToMultiByte(CP_UTF8,0,v->name,-1,app.profile.interface_name,sizeof(app.profile.interface_name),NULL,NULL)) app.profile.interface_name[0] = 0;
    if (!app.custom_profile) { network_range(v->ip,v->prefix,&app.profile.ranges[0]); app.profile.range_count = 1; }
}
static void populate_saved_profiles(void) {
    bool previous = app.loading_tree; app.loading_tree = true;
    if (app.profile_root) TreeView_DeleteItem(app.tree,app.profile_root);
    app.saved_count = win_saved_profiles(app.saved_profiles,32);
    TVINSERTSTRUCTW t = {0}; t.hParent = TVI_ROOT; t.hInsertAfter = TVI_LAST; t.item.mask = TVIF_TEXT|TVIF_PARAM; t.item.pszText = (wchar_t *)tr(L"Saved profiles",L"Сохранённые профили"); t.item.lParam = -10000;
    app.profile_root = TreeView_InsertItem(app.tree,&t);
    for (size_t i = 0; i < app.saved_count; ++i) { t.hParent = app.profile_root; t.item.pszText = app.saved_profiles[i].name; t.item.lParam = -(LPARAM)(i+2); TreeView_InsertItem(app.tree,&t); }
    TreeView_Expand(app.tree,app.profile_root,TVE_EXPAND); app.loading_tree = previous;
}
static void enumerate(void) {
    if (app.scanning || app.monitoring) return;
    app.selected = -1; app.loading_tree = true; TreeView_DeleteAllItems(app.tree); app.profile_root = NULL; app.loading_tree = false;
    DWORD error = win_enumerate(app.interfaces,MAX_INTERFACES,&app.interface_count);
    if (error) { wchar_t msg[128]; swprintf_s(msg,128,L"%ls (%lu)",tr(L"Cannot enumerate interfaces",L"Не удалось получить интерфейсы"),error); MessageBoxW(app.window,msg,L"Look@LAN-ng",MB_ICONWARNING); }
    HTREEITEM selected = NULL; uint32_t best = UINT32_MAX;
    for (size_t i = 0; i < app.interface_count; ++i) {
        win_interface_t *v = &app.interfaces[i]; wchar_t text[256],ip[32]; ip_text(v->ip,ip,32);
        swprintf_s(text,256,L"%ls%ls",v->name,v->automatic ? L"" : tr(L" (manual)",L" (вручную)"));
        TVINSERTSTRUCTW t = {0}; t.hParent = TVI_ROOT; t.hInsertAfter = TVI_LAST; t.item.mask = TVIF_TEXT | TVIF_PARAM; t.item.pszText = text; t.item.lParam = (LPARAM)i;
        HTREEITEM parent = TreeView_InsertItem(app.tree,&t);
        swprintf_s(text,256,L"%ls/%u",ip,v->prefix); t.hParent = parent; t.item.pszText = text; HTREEITEM child = TreeView_InsertItem(app.tree,&t);
        app.interface_items[i] = child;
        TreeView_Expand(app.tree,parent,TVE_EXPAND);
        if (app.custom_profile && app.profile.interface_id[0]) { if (!strcmp(v->id,app.profile.interface_id)) selected = child; }
        else if (v->automatic && (!selected || v->metric < best)) { selected = child; best = v->metric; }
    }
    if (selected) TreeView_SelectItem(app.tree,selected);
    populate_saved_profiles();
    update_summary(); update_status();
}
static void scan_started(ipv4_range_t range, bool track_absent) {
    app.generation = app.backend.generation; app.scanning = true; app.started = GetTickCount64(); app.elapsed = 0; app.round_healthy = true;
    monitor_begin(&app.model,range,app.interfaces[app.selected].index,track_absent,(uint64_t)time(NULL));
    sync_log(false); show_detail(); update_status();
}
static void profile_scan_started(void) {
    app.generation = app.backend.generation; app.scanning = true; app.started = GetTickCount64(); app.elapsed = 0; app.round_healthy = true;
    monitor_begin_ranges(&app.model,app.backend.ranges,app.backend.range_count,app.interfaces[app.selected].index,true,(uint64_t)time(NULL));
    sync_log(false); show_detail(); update_status();
}
static bool confirm_large_scan(const wchar_t *text) {
    app.dialog_open = true;
    int answer = MessageBoxW(app.window,text,L"Look@LAN-ng",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
    app.dialog_open = false; return answer == IDYES;
}
static void begin_scan(void) {
    if (app.scanning || app.tools_job || app.closing || app.selected < 0) return;
    char error[128]; if (!profile_validate(&app.profile,error,sizeof(error))) { MessageBoxW(app.window,tr(L"Configure a valid scan profile first.",L"Сначала настройте корректный профиль сканирования."),L"Look@LAN-ng",MB_ICONWARNING); return; }
    uint64_t total; if (!ranges_normalize(app.profile.ranges,&app.profile.range_count,&total)) return;
    if (total > INT32_MAX) { MessageBoxW(app.window,tr(L"The selected ranges are too large. Reduce them in the profile.",L"Выбранные диапазоны слишком велики. Уменьшите их в профиле."),L"Look@LAN-ng",MB_ICONWARNING); return; }
    if (total > 4096 && !confirm_large_scan(tr(L"The profile contains more than 4096 addresses. Scan all selected ranges?",L"Профиль содержит более 4096 адресов. Сканировать все выбранные диапазоны?"))) return;
    if (app.profile.port_count > 4096 && !confirm_large_scan(tr(L"This profile checks more than 4096 TCP ports for every discovered host. Continue?",L"Профиль проверяет более 4096 TCP-портов для каждого найденного узла. Продолжить?"))) return;
    if (!win_profile_scan_start(&app.backend,&app.interfaces[app.selected],&app.profile,false,NULL,0)) { MessageBoxW(app.window,tr(L"Cannot start scanner.",L"Не удалось запустить сканирование."),L"Look@LAN-ng",MB_ICONERROR); return; }
    app.monitor_range = app.profile.ranges[0]; app.has_monitor_range = true; profile_scan_started();
}
static void toggle_monitoring(void) {
    if (app.closing || app.selected < 0) return;
    app.monitoring = !app.monitoring;
    if (app.monitoring) {
        if (!app.has_monitor_range && !app.scanning) begin_scan();
        if (!app.has_monitor_range) app.monitoring = false;
        app.next_monitor = GetTickCount64()+(ULONGLONG)app.monitor_interval*1000;
    } else if (app.scanning && app.backend.monitoring) win_scan_stop(&app.backend);
    update_summary(); update_status();
}
static void monitoring_tick(void) {
    if (!app.monitoring || !app.has_monitor_range || app.scanning || app.tools_job || app.closing || app.dialog_open || win_scan_running(&app.backend) || GetTickCount64() < app.next_monitor) return;
    if (win_profile_scan_start(&app.backend,&app.interfaces[app.selected],&app.profile,true,app.hosts,app.host_count)) profile_scan_started();
    else {
        app.monitoring = false; monitor_note(&app.model,MON_SCAN_ERROR,"Cannot start monitoring worker",(uint64_t)time(NULL));
        sync_log(false); update_summary(); update_status();
    }
}
typedef struct { const wchar_t *title, *prompt; wchar_t *text; int capacity; } input_dialog_t;
static INT_PTR CALLBACK input_proc(HWND dialog, UINT message, WPARAM w, LPARAM l) {
    input_dialog_t *input = (input_dialog_t *)GetWindowLongPtrW(dialog,DWLP_USER);
    if (message == WM_INITDIALOG) {
        input = (input_dialog_t *)l; SetWindowLongPtrW(dialog,DWLP_USER,l);
        SetWindowTextW(dialog,input->title); SetDlgItemTextW(dialog,IDC_PROMPT,input->prompt); SetDlgItemTextW(dialog,IDC_INPUT,input->text);
        SetDlgItemTextW(dialog,IDCANCEL,tr(L"Cancel",L"Отмена")); SendDlgItemMessageW(dialog,IDC_INPUT,EM_SETLIMITTEXT,input->capacity-1,0);
        SetFocus(GetDlgItem(dialog,IDC_INPUT)); SendDlgItemMessageW(dialog,IDC_INPUT,EM_SETSEL,0,-1); return FALSE;
    }
    if (message == WM_COMMAND && LOWORD(w) == IDOK && input) {
        GetDlgItemTextW(dialog,IDC_INPUT,input->text,input->capacity);
        if (!input->text[0]) return TRUE;
        EndDialog(dialog,IDOK); return TRUE;
    }
    if ((message == WM_COMMAND && LOWORD(w) == IDCANCEL) || message == WM_CLOSE) { EndDialog(dialog,IDCANCEL); return TRUE; }
    return FALSE;
}
static bool input_box(const wchar_t *title, const wchar_t *prompt, wchar_t *text, int capacity) {
    input_dialog_t input = {title,prompt,text,capacity};
    app.dialog_open = true;
    bool accepted = DialogBoxParamW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(IDD_INPUT),app.window,input_proc,(LPARAM)&input) == IDOK;
    app.dialog_open = false; return accepted;
}
static void single_scan(bool selected, bool custom) {
    if (app.scanning || app.tools_job || app.closing || app.selected < 0) return;
    wchar_t target[256] = L"";
    size_t index = selected ? selected_host() : SIZE_MAX;
    if (index != SIZE_MAX) ip_text(ipv4_value(&app.hosts[index].address),target,256);
    else if (!input_box(tr(L"Single host scan",L"Сканирование узла"),tr(L"IPv4 address or host name:",L"IPv4-адрес или имя узла:"),target,256)) return;
    uint16_t *ports = NULL; size_t count = quick_port_count;
    const uint16_t *scan_ports = quick_ports;
    if (custom) {
        wchar_t text[4097] = L"80,443,8080";
        if (!input_box(tr(L"Custom TCP ports",L"Произвольные TCP-порты"),tr(L"Examples: 1-1024 or 20-25,80,443",L"Примеры: 1-1024 или 20-25,80,443"),text,4097)) return;
        char ascii[4097]; ports = malloc(65535*sizeof(*ports));
        int n = WideCharToMultiByte(CP_UTF8,0,text,-1,ascii,sizeof(ascii),NULL,NULL);
        if (!ports || !n || !ports_parse(ascii,(size_t)n-1,ports,65535,&count)) {
            free(ports); MessageBoxW(app.window,tr(L"Invalid port list. Use ports 1–65535 and ascending ranges.",L"Некорректный список. Допустимы порты 1–65535 и возрастающие диапазоны."),L"Look@LAN-ng",MB_ICONWARNING); return;
        }
        if (count > 4096 && !confirm_large_scan(tr(L"More than 4096 TCP ports selected. Continue?",L"Выбрано более 4096 TCP-портов. Продолжить?"))) { free(ports); return; }
        scan_ports = ports;
    }
    bool started = win_profile_host_start(&app.backend,&app.interfaces[app.selected],target,scan_ports,count,&app.profile);
    free(ports);
    if (!started) { MessageBoxW(app.window,tr(L"Cannot start scanner.",L"Не удалось запустить сканирование."),L"Look@LAN-ng",MB_ICONERROR); return; }
    char address[256]; uint32_t ip = 0; int length = WideCharToMultiByte(CP_UTF8,0,target,-1,address,256,NULL,NULL);
    bool numeric = length && ipv4_parse(address,(size_t)length-1,&ip);
    ipv4_range_t scope = {numeric ? ip : 0,numeric ? ip : 0};
    if (index == SIZE_MAX) SetWindowTextW(app.filter,L"");
    scan_started(scope,numeric);
}
static void profile_error(const wchar_t *action, const char *detail) {
    wchar_t extra[256] = L"", message[512]; DWORD error = GetLastError(); if (detail && detail[0]) utf8_text(detail,extra,256);
    swprintf_s(message,512,L"%ls (%lu)\n%ls",action,error,extra); MessageBoxW(app.window,message,L"Look@LAN-ng",MB_ICONWARNING);
}
static void load_profile_path(const wchar_t *path) {
    if (app.scanning || app.monitoring || app.closing) return;
    profile_t *candidate = malloc(sizeof(*candidate)); if (!candidate) return;
    char error[256] = "";
    if (!win_profile_load(path,candidate,error,sizeof(error))) { free(candidate); profile_error(tr(L"Cannot load profile",L"Не удалось загрузить профиль"),error); return; }
    int selected = app.selected;
    if (candidate->interface_id[0]) { selected = -1; for (size_t i = 0; i < app.interface_count; ++i) if (!strcmp(candidate->interface_id,app.interfaces[i].id)) { selected = (int)i; break; } }
    app.profile = *candidate; free(candidate); app.custom_profile = true; app.monitor_interval = app.profile.monitor_interval_sec; app.selected = selected;
    wcscpy_s(app.profile_path,MAX_PATH,path); reset_session();
    app.loading_tree = true; TreeView_SelectItem(app.tree,selected >= 0 ? app.interface_items[selected] : NULL); app.loading_tree = false;
    bind_profile_interface(); localize();
    if (selected < 0) MessageBoxW(app.window,tr(L"The saved interface is unavailable. Select a network interface before scanning.",L"Сохранённый адаптер недоступен. Выберите сетевой интерфейс перед сканированием."),L"Look@LAN-ng",MB_ICONINFORMATION);
}
static void profile_command(int command) {
    if (app.closing || ((command != ID_PROFILE_SAVE) && (app.scanning || app.monitoring))) return;
    if (command == ID_PROFILE_OPEN) {
        wchar_t path[MAX_PATH] = L""; app.dialog_open = true; bool accepted = win_profile_file_dialog(app.window,false,app.russian,path); app.dialog_open = false;
        if (accepted) load_profile_path(path); return;
    }
    if (command == ID_PROFILE_NEW) {
        profile_t *candidate = malloc(sizeof(*candidate)); if (!candidate) return; profile_defaults(candidate); candidate->monitor_interval_sec = app.monitor_interval;
        if (app.selected >= 0) { network_range(app.interfaces[app.selected].ip,app.interfaces[app.selected].prefix,&candidate->ranges[0]); candidate->range_count = 1; }
        app.dialog_open = true; bool accepted = win_profile_edit(app.window,app.russian,candidate); app.dialog_open = false;
        if (accepted) { app.profile = *candidate; app.custom_profile = true; app.profile_path[0] = 0; app.monitor_interval = app.profile.monitor_interval_sec; bind_profile_interface(); reset_session(); localize(); }
        free(candidate); return;
    }
    if (command == ID_PROFILE_EDIT) {
        app.dialog_open = true; bool changed = win_profile_edit(app.window,app.russian,&app.profile); app.dialog_open = false;
        if (changed) { app.custom_profile = true; app.monitor_interval = app.profile.monitor_interval_sec; bind_profile_interface(); reset_session(); localize(); }
        return;
    }
    if (command == ID_PROFILE_SAVE) {
        char error[128]; if (!profile_validate(&app.profile,error,sizeof(error))) { SetLastError(ERROR_INVALID_DATA); profile_error(tr(L"Configure a profile before saving",L"Настройте профиль перед сохранением"),error); return; }
        wchar_t path[MAX_PATH]; wcscpy_s(path,MAX_PATH,app.profile_path);
        if (!path[0]) {
            wchar_t name[256]; utf8_text(app.profile.name,name,256);
            for (size_t i = 0; name[i]; ++i) if (name[i] < 32 || wcschr(L"<>:\"/\\|?*",name[i])) name[i] = L'_';
            swprintf_s(path,MAX_PATH,L"%.180ls.lanprofile",name);
        }
        app.dialog_open = true; bool accepted = win_profile_file_dialog(app.window,true,app.russian,path); app.dialog_open = false;
        if (!accepted) return;
        if (!win_profile_save(path,&app.profile)) { profile_error(tr(L"Cannot save profile",L"Не удалось сохранить профиль"),NULL); return; }
        wcscpy_s(app.profile_path,MAX_PATH,path); populate_saved_profiles();
    }
}
static void export_hosts(report_format_t format) {
    if (app.closing || app.export_job) return;
    const wchar_t *extension = format == REPORT_CSV ? L"csv" : format == REPORT_TXT ? L"txt" : L"html";
    wchar_t path[MAX_PATH]; swprintf_s(path,MAX_PATH,L"Look@LAN-ng-report.%ls",extension);
    app.dialog_open = true; bool accepted = win_report_file_dialog(app.window,format,app.russian,path); app.dialog_open = false;
    if (!accepted) return;
    report_options_t options = {0}; options.russian = app.russian; memcpy(options.profile_name,app.profile.name,sizeof(options.profile_name));
    WideCharToMultiByte(CP_UTF8,0,APP_COPYRIGHT_W,-1,options.copyright,sizeof(options.copyright),NULL,NULL);
    app.export_job = win_export_start(path,format,app.hosts,app.host_count,&app.vendors,&options);
    if (!app.export_job) { MessageBoxW(app.window,tr(L"Cannot start export.",L"Не удалось начать экспорт."),L"Look@LAN-ng",MB_ICONERROR); return; }
    monitor_note(&app.model,MON_EXPORT_STARTED,format == REPORT_CSV ? "CSV" : format == REPORT_TXT ? "TXT" : "HTML",(uint64_t)time(NULL)); sync_log(false); update_status();
}
static void export_tick(void) {
    DWORD error;
    if (!app.export_job || !win_export_done(app.export_job,&error)) return;
    win_export_release(app.export_job); app.export_job = NULL;
    monitor_note(&app.model,error ? MON_EXPORT_ERROR : MON_EXPORT_FINISHED,error ? "File write failed / cancelled" : "Saved",(uint64_t)time(NULL)); sync_log(false); update_status();
    if (error && !app.closing) { SetLastError(error); profile_error(tr(L"Export failed",L"Ошибка экспорта"),NULL); }
}
static void export_menu(void) {
    if (app.export_job || app.closing) return;
    HMENU menu = CreatePopupMenu(); AppendMenuW(menu,MF_STRING,ID_EXPORT_CSV,L"CSV"); AppendMenuW(menu,MF_STRING,ID_EXPORT_TXT,L"TXT"); AppendMenuW(menu,MF_STRING,ID_EXPORT_HTML,L"HTML");
    RECT r; SendMessageW(app.toolbar,TB_GETRECT,ID_EXPORT_MENU,(LPARAM)&r); MapWindowPoints(app.toolbar,NULL,(POINT *)&r,2);
    int action = TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,r.left,r.bottom,0,app.window,NULL); DestroyMenu(menu); if (action) SendMessageW(app.window,WM_COMMAND,action,0);
}
static void paint_statistics(const DRAWITEMSTRUCT *draw) {
    HDC dc = draw->hDC; RECT area = draw->rcItem; FillRect(dc,&area,(HBRUSH)GetStockObject(WHITE_BRUSH));
    HGDIOBJ old = SelectObject(dc,app.font); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,GetSysColor(COLOR_WINDOWTEXT));
    statistics_t s = statistics_compute(app.hosts,app.host_count,app.generation ? (uint32_t)app.backend.completed : 0);
    const wchar_t *left[] = {tr(L"Scanned addresses",L"Проверено адресов"),tr(L"Online",L"В сети"),tr(L"Offline",L"Не в сети"),tr(L"New",L"Новые"),tr(L"Unconfirmed",L"Неподтверждённые")};
    const wchar_t *right[] = {tr(L"Table records",L"Узлов в таблице"),L"Windows",L"Linux / Unix",tr(L"Network devices",L"Сетевые устройства"),tr(L"Unknown OS (online)",L"Неизвестная ОС (в сети)")};
    size_t values[] = {s.scanned,s.online,s.offline,s.fresh,s.unknown}, os[] = {s.records,s.windows,s.unix_hosts,s.network_devices,s.unknown_os};
    int middle = (area.left+area.right)/2;
    for (unsigned i = 0; i < 5; ++i) {
        int y = area.top+px(8)+(int)i*px(23); wchar_t text[256]; RECT r = {area.left+px(8),y,middle-px(8),y+px(20)};
        swprintf_s(text,256,L"%ls: %zu",left[i],values[i]); DrawTextW(dc,text,-1,&r,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        r.left = middle+px(8); r.right = area.right-px(8); swprintf_s(text,256,L"%ls: %zu",right[i],os[i]); DrawTextW(dc,text,-1,&r,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        if (i >= 1 && i <= 3) {
            RECT bar = {area.left+px(8),y+px(18),middle-px(12),y+px(21)}; FillRect(dc,&bar,(HBRUSH)GetStockObject(LTGRAY_BRUSH));
            bar.right = bar.left+(int)((uint64_t)(middle-px(12)-bar.left)*values[i]/(s.records ? s.records : 1));
            HBRUSH color = CreateSolidBrush(i == 1 ? RGB(35,148,87) : i == 2 ? RGB(140,148,157) : RGB(50,114,200)); FillRect(dc,&bar,color); DeleteObject(color);
        }
    }
    SelectObject(dc,old);
}
static void copy_text(const wchar_t *text) {
    if (!text[0] || !OpenClipboard(app.window)) return;
    size_t bytes = (wcslen(text)+1)*sizeof(*text);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE,bytes);
    if (memory) {
        void *pointer = GlobalLock(memory);
        if (pointer) { memcpy(pointer,text,bytes); GlobalUnlock(memory); if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT,memory)) memory = NULL; }
        if (memory) GlobalFree(memory);
    }
    CloseClipboard();
}
static void start_tool(bool trace) {
    size_t index = selected_host(); if (index == SIZE_MAX || app.selected < 0 || app.scanning || app.closing || app.tools_job) return;
    uint32_t target = ipv4_value(&app.hosts[index].address);
    app.tools_job = win_tools_start(&app.backend.ping,app.interfaces[app.selected].ip,target,app.hosts[index].interface_index,app.profile.icmp_timeout_ms,trace);
    if (!app.tools_job) { MessageBoxW(app.window,tr(L"Cannot start diagnostic.",L"Не удалось запустить диагностику."),L"Look@LAN-ng",MB_ICONERROR); return; }
    app.tools_trace = trace; app.tools_session = app.session;
    if (trace) { app.trace_target = target; app.trace_interface = app.hosts[index].interface_index; app.trace_count = 0; }
    TabCtrl_SetCurSel(app.tabs,trace ? 6 : 2); show_detail(); update_status();
}
static void tools_tick(void) {
    if (!app.tools_job) return;
    bool finished = win_tools_finished(app.tools_job); ping_sample_t sample;
    while (win_tools_pop(app.tools_job,&sample)) if (app.tools_session == app.session) {
        if (app.tools_trace) { if (app.trace_count < 30) app.trace[app.trace_count++] = sample; }
        else details_ping_add(&app.details,sample);
    }
    if (finished) { win_tools_free(app.tools_job); app.tools_job = NULL; update_status(); }
    show_detail();
}
static void host_action(int action) {
    size_t index = selected_host(); if (index == SIZE_MAX) return;
    wchar_t ip[32], target[512]; ip_text(ipv4_value(&app.hosts[index].address),ip,32);
    if (action == ID_COPY_IP) { copy_text(ip); return; }
    if (action == ID_COPY_NAME) { utf8_text(app.hosts[index].hostname,target,512); copy_text(target); return; }
    if (action == ID_COPY_MAC) { row_text(&app.hosts[index],3,target,512); copy_text(target); return; }
    if (action == ID_PROPERTIES) { TabCtrl_SetCurSel(app.tabs,0); show_detail(); SetFocus(app.detail); return; }
    if (action == ID_ACKNOWLEDGE) { monitor_acknowledge(&app.model,index); update_row(index); show_detail(); update_status(); return; }
    if (action == ID_NOTES) {
        wchar_t note[512]; utf8_text(details_note(&app.details,ipv4_value(&app.hosts[index].address),app.hosts[index].interface_index),note,512);
        if (input_box(tr(L"Device notes (session)",L"Заметки устройства (сессия)"),tr(L"Notes, up to 511 UTF-8 bytes:",L"Заметки, до 511 байт UTF-8:"),note,512)) {
            char utf8[512]; if (!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,note,-1,utf8,512,NULL,NULL) || !details_note_set(&app.details,ipv4_value(&app.hosts[index].address),app.hosts[index].interface_index,utf8)) MessageBoxW(app.window,tr(L"Note too long or notes capacity reached.",L"Заметка слишком длинная или достигнут лимит заметок."),L"Look@LAN-ng",MB_ICONWARNING);
            show_detail();
        } return;
    }
    const wchar_t *file = target, *parameters = NULL;
    wchar_t system[MAX_PATH];
    if (action == ID_WEB || action == ID_HTTPS) {
        uint16_t port = action == ID_WEB ? 80 : 443;
        for (unsigned i = 0; i < app.hosts[index].port_count; ++i) if (app.hosts[index].open_ports[i] == (action == ID_WEB ? 8080 : 8443)) port = app.hosts[index].open_ports[i];
        swprintf_s(target,512,L"%ls://%ls:%u/",action == ID_WEB ? L"http" : L"https",ip,port);
    } else if (action == ID_SMB) swprintf_s(target,512,L"\\\\%ls",ip);
    else if (action == ID_RDP) {
        UINT n = GetSystemDirectoryW(system,MAX_PATH); if (!n || n >= MAX_PATH-12) return;
        wcscat_s(system,MAX_PATH,L"\\mstsc.exe"); file = system; swprintf_s(target,512,L"/v:%ls",ip); parameters = target;
    } else if (action == ID_SSH) {
        UINT n = GetSystemDirectoryW(system,MAX_PATH); if (!n || n >= MAX_PATH-20) return;
        wcscat_s(system,MAX_PATH,L"\\OpenSSH\\ssh.exe"); if (GetFileAttributesW(system) == INVALID_FILE_ATTRIBUTES) { MessageBoxW(app.window,tr(L"Windows OpenSSH client is not installed.",L"Клиент Windows OpenSSH не установлен."),L"Look@LAN-ng",MB_ICONINFORMATION); return; }
        wchar_t user[64] = L""; if (!input_box(tr(L"Open SSH",L"Открыть SSH"),tr(L"User name (empty uses the Windows user):",L"Имя пользователя (пусто — пользователь Windows):"),user,64)) return;
        for (size_t i = 0; user[i]; ++i) if (!(user[i] >= L'a' && user[i] <= L'z') && !(user[i] >= L'A' && user[i] <= L'Z') && !(user[i] >= L'0' && user[i] <= L'9') && user[i] != L'_' && user[i] != L'.' && user[i] != L'-') { MessageBoxW(app.window,tr(L"Use letters, digits, _, . or -.",L"Допустимы буквы, цифры, _, . и -."),L"Look@LAN-ng",MB_ICONWARNING); return; }
        if (user[0] == L'-') return;
        swprintf_s(target,512,L"%ls%ls%ls",user,user[0] ? L"@" : L"",ip); file = system; parameters = target;
    } else return;
    HINSTANCE result = ShellExecuteW(app.window,L"open",file,parameters,NULL,SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) MessageBoxW(app.window,tr(L"Windows could not open this action.",L"Windows не удалось выполнить действие."),L"Look@LAN-ng",MB_ICONWARNING);
}
static void context_menu(POINT point) {
    if (point.x == -1 && point.y == -1) { RECT r; GetWindowRect(app.list,&r); point.x = r.left+px(50); point.y = r.top+px(45); }
    else {
        POINT local = point; ScreenToClient(app.list,&local); LVHITTESTINFO hit = {0}; hit.pt = local;
        int row = ListView_HitTest(app.list,&hit); if (row >= 0) ListView_SetItemState(app.list,row,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    }
    if (selected_host() == SIZE_MAX) return;
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu,MF_STRING|(app.tools_job || app.scanning ? MF_GRAYED : 0),ID_PING,L"Ping");
    AppendMenuW(menu,MF_STRING|(app.tools_job || app.scanning ? MF_GRAYED : 0),ID_TRACE,tr(L"Traceroute",L"Трассировка"));
    AppendMenuW(menu,MF_STRING | (app.scanning ? MF_GRAYED : 0),ID_PORTS,tr(L"Scan Ports",L"Сканировать порты"));
    AppendMenuW(menu,MF_STRING | (app.scanning ? MF_GRAYED : 0),ID_CUSTOM,tr(L"Custom ports…",L"Произвольные порты…"));
    AppendMenuW(menu,MF_STRING | (app.scanning ? MF_GRAYED : 0),ID_RESCAN,tr(L"Rescan Host",L"Повторно проверить узел"));
    AppendMenuW(menu,MF_STRING,ID_ACKNOWLEDGE,tr(L"Mark as seen",L"Отметить просмотренным"));
    AppendMenuW(menu,MF_SEPARATOR,0,NULL);
    AppendMenuW(menu,MF_STRING,ID_WEB,tr(L"Open Web",L"Открыть Web")); AppendMenuW(menu,MF_STRING,ID_HTTPS,tr(L"Open HTTPS",L"Открыть HTTPS"));
    AppendMenuW(menu,MF_STRING,ID_RDP,tr(L"Open RDP",L"Открыть RDP")); AppendMenuW(menu,MF_STRING,ID_SMB,tr(L"Open SMB share",L"Открыть SMB-ресурс"));
    AppendMenuW(menu,MF_STRING,ID_SSH,tr(L"Open SSH",L"Открыть SSH")); AppendMenuW(menu,MF_STRING,ID_NOTES,tr(L"Device notes…",L"Заметки устройства…"));
    AppendMenuW(menu,MF_SEPARATOR,0,NULL);
    AppendMenuW(menu,MF_STRING,ID_COPY_IP,tr(L"Copy IP",L"Копировать IP")); AppendMenuW(menu,MF_STRING,ID_COPY_NAME,tr(L"Copy Hostname",L"Копировать имя узла")); AppendMenuW(menu,MF_STRING,ID_COPY_MAC,tr(L"Copy MAC",L"Копировать MAC"));
    AppendMenuW(menu,MF_STRING,ID_PROPERTIES,tr(L"Properties",L"Свойства"));
    int action = TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,app.window,NULL); DestroyMenu(menu);
    if (action) SendMessageW(app.window,WM_COMMAND,(WPARAM)action,0);
}
static void drain_events(void) {
    net_event_t event; bool changed = false;
    if (!win_event_pop(&app.backend,&event)) {
        if (app.scanning || app.closing || app.monitoring) update_status();
        if (app.closing && !app.tools_job && !app.export_job && win_backend_release(&app.backend)) DestroyWindow(app.window);
        return;
    }
    SendMessageW(app.list,WM_SETREDRAW,FALSE,0);
    for (unsigned batch = 0; batch < 256; ++batch) {
        if (batch && !win_event_pop(&app.backend,&event)) break;
        if (event.generation != app.generation) continue;
        if (event.type == NET_PING) {
            ping_sample_t sample = {0}; sample.ip = ipv4_value(&event.host.address); sample.interface_index = event.host.interface_index; sample.timestamp = (uint64_t)time(NULL); sample.received = event.host.has_latency; sample.latency_ms = event.host.latency_ms; sample.status = event.error;
            details_ping_add(&app.details,sample); changed = true;
        } else if (event.type == NET_HOST || event.type == NET_DNS) {
            if (event.type == NET_DNS && app.backend.cancel) continue;
            size_t index = monitor_observe(&app.model,&event.host,event.type == NET_HOST && event.host.online,(uint64_t)time(NULL));
            if (index != SIZE_MAX) { update_row(index); changed = true; }
        } else if (event.type == NET_DONE) {
            app.elapsed = GetTickCount64() - app.started; app.scanning = false;
            monitor_finish(&app.model,!event.cancelled && app.round_healthy && event.completed == app.backend.total,(uint64_t)time(NULL));
            for (size_t i = 0; i < app.host_count; ++i) update_row(i);
            ListView_SortItems(app.list,compare_rows,0); changed = true;
            app.next_monitor = GetTickCount64()+(ULONGLONG)app.monitor_interval*1000;
        } else if (event.type == NET_ERROR) {
            app.round_healthy = false; char note[128]; snprintf(note,128,"Windows error %lu",event.error); monitor_note(&app.model,MON_SCAN_ERROR,note,(uint64_t)time(NULL));
            if (app.backend.monitoring) { app.monitoring = false; update_summary(); }
            else if (!app.closing) { wchar_t text[256]; swprintf_s(text,256,L"%ls (%lu)",tr(L"Scan failed: host lookup or worker startup error",L"Ошибка сканирования: разрешение имени или запуск рабочих потоков"),event.error); MessageBoxW(app.window,text,L"Look@LAN-ng",MB_ICONERROR); }
        }
    }
    SendMessageW(app.list,WM_SETREDRAW,TRUE,0);
    sync_log(false);
    if (changed) { InvalidateRect(app.list,NULL,FALSE); show_detail(); }
    update_status();
    if (app.closing && !app.tools_job && !app.export_job && win_backend_release(&app.backend)) DestroyWindow(app.window);
}
static void settings_path(void) {
    GetModuleFileNameW(NULL,app.ini,MAX_PATH);
    wchar_t *slash = wcsrchr(app.ini,L'\\');
    if (slash) swprintf_s(slash,(size_t)(MAX_PATH-(slash-app.ini)),L"\\Look@LAN-ng.ini");
    wchar_t fallback[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA",fallback,MAX_PATH);
    if (GetFileAttributesW(app.ini) == INVALID_FILE_ATTRIBUTES && n && n < MAX_PATH-32) {
        wcscat_s(fallback,MAX_PATH,L"\\Look@LAN-ng\\Look@LAN-ng.ini");
        if (GetFileAttributesW(fallback) != INVALID_FILE_ATTRIBUTES) wcscpy_s(app.ini,MAX_PATH,fallback);
    }
}
static void save_settings(void) {
    if (app.smoke) return;
    const wchar_t *language = app.system_language ? L"system" : app.russian ? L"ru" : L"en";
    if (!WritePrivateProfileStringW(L"UI",L"Language",language,app.ini)) {
        DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA",app.ini,MAX_PATH);
        if (!n || n >= MAX_PATH-40) return;
        wcscat_s(app.ini,MAX_PATH,L"\\Look@LAN-ng"); CreateDirectoryW(app.ini,NULL);
        wcscat_s(app.ini,MAX_PATH,L"\\Look@LAN-ng.ini");
        if (!WritePrivateProfileStringW(L"UI",L"Language",language,app.ini)) return;
    }
    for (int i = 0; i < 10; ++i) { wchar_t key[32],value[32]; swprintf_s(key,32,L"Column%d",i); swprintf_s(value,32,L"%d",MulDiv(ListView_GetColumnWidth(app.list,i),96,(int)app.dpi)); WritePrivateProfileStringW(L"UI",key,value,app.ini); }
    wchar_t interval[32]; swprintf_s(interval,32,L"%u",app.monitor_interval); WritePrivateProfileStringW(L"UI",L"MonitorInterval",interval,app.ini);
}
static HIMAGELIST status_images(void) {
    int size = px(14); HIMAGELIST list = ImageList_Create(size,size,ILC_COLOR32,2,0);
    BITMAPINFO info = {0}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = size; info.bmiHeader.biHeight = -size;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    uint32_t *pixels = NULL; HDC screen = GetDC(app.window); HBITMAP bitmap = CreateDIBSection(screen,&info,DIB_RGB_COLORS,(void **)&pixels,NULL,0); ReleaseDC(app.window,screen);
    if (!list || !bitmap) { if (list) ImageList_Destroy(list); if (bitmap) DeleteObject(bitmap); return NULL; }
    for (int state = 0; state < 2; ++state) {
        unsigned red = state ? 146 : 35, green = state ? 153 : 148, blue = state ? 160 : 87;
        int radius = (size-2*px(3))*4;
        for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
            unsigned coverage = 0;
            for (int sy = 0; sy < 4; ++sy) for (int sx = 0; sx < 4; ++sx) { int dx = x*8+sx*2+1-size*4, dy = y*8+sy*2+1-size*4; if (dx*dx+dy*dy <= radius*radius) ++coverage; }
            unsigned alpha = coverage*255/16;
            pixels[y*size+x] = (alpha<<24)|((red*alpha/255)<<16)|((green*alpha/255)<<8)|(blue*alpha/255);
        }
        ImageList_Add(list,bitmap,NULL);
    }
    DeleteObject(bitmap); return list;
}
static HWND control(const wchar_t *class_name, DWORD style, DWORD extra) {
    return CreateWindowExW(extra,class_name,L"",WS_CHILD|WS_VISIBLE|style,0,0,0,0,app.window,NULL,GetModuleHandleW(NULL),NULL);
}
static bool dpi_smoke(void) {
    UINT initial = app.dpi; bool russian = app.russian;
    RECT original; GetWindowRect(app.window,&original);
    DWORD gdi_before = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS), user_before = GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
    const UINT scales[] = {96,120,144,168,192};
    for (unsigned pass = 0; pass < 20; ++pass) for (size_t scale = 0; scale < 5; ++scale) {
        UINT dpi = scales[scale]; app.russian = (pass & 1) != 0;
        RECT target = {0,0,MulDiv(pass & 2 ? 760 : 1100,(int)dpi,96),MulDiv(pass & 2 ? 520 : 700,(int)dpi,96)};
        SendMessageW(app.window,WM_DPICHANGED,MAKELONG(dpi,dpi),(LPARAM)&target); localize();
        HDC icon_dc = GetDC(app.toolbar);
        for (unsigned icon = 0; icon < 6; ++icon) for (unsigned enabled = 0; enabled < 2; ++enabled)
            win_toolbar_icon(icon_dc,0,0,px(20),icon,enabled != 0,GetSysColor(COLOR_BTNFACE));
        ReleaseDC(app.toolbar,icon_dc); InvalidateRect(app.toolbar,NULL,TRUE);
        HWND controls[] = {app.tree,app.list,app.tabs,app.filter,app.statistics_view};
        for (size_t i = 0; i < sizeof(controls)/sizeof(controls[0]); ++i) {
            RECT bounds; GetWindowRect(controls[i],&bounds);
            if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) return false;
        }
        RECT button, filter; SendMessageW(app.toolbar,TB_GETITEMRECT,5,(LPARAM)&button);
        MapWindowPoints(app.toolbar,app.window,(POINT *)&button,2); GetWindowRect(app.filter,&filter); MapWindowPoints(NULL,app.window,(POINT *)&filter,2);
        if (filter.top < button.bottom && filter.left < button.right) return false;
    }
    app.russian = russian; SendMessageW(app.window,WM_DPICHANGED,MAKELONG(initial,initial),(LPARAM)&original); localize();
    DWORD gdi_after = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS), user_after = GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
    return gdi_after <= gdi_before+2 && user_after <= user_before+2;
}
static bool preview_bitmap(void) {
    RECT bounds; GetClientRect(app.window,&bounds);
    BITMAPINFO info = {0}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = bounds.right; info.bmiHeader.biHeight = bounds.bottom;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(app.window), dc = CreateCompatibleDC(screen); void *pixels = NULL;
    HBITMAP bitmap = CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,NULL,0); ReleaseDC(app.window,screen);
    if (!bitmap || !dc) { if (bitmap) DeleteObject(bitmap); if (dc) DeleteDC(dc); return false; }
    HGDIOBJ previous = SelectObject(dc,bitmap);
    RECT original; GetWindowRect(app.window,&original);
    SetWindowPos(app.window,NULL,-30000,-30000,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
    ShowWindow(app.window,SW_SHOWNOACTIVATE); RedrawWindow(app.window,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    PrintWindow(app.window,dc,PW_CLIENTONLY);
    ShowWindow(app.window,SW_HIDE); SetWindowPos(app.window,NULL,original.left,original.top,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
    GdiFlush();
    DWORD bytes = (DWORD)bounds.right*(DWORD)bounds.bottom*4;
    BITMAPFILEHEADER header = {0}; header.bfType = 0x4d42; header.bfOffBits = sizeof(header)+sizeof(BITMAPINFOHEADER); header.bfSize = header.bfOffBits+bytes;
    HANDLE file = CreateFileW(L"build\\ui-preview.bmp",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL); DWORD written;
    bool ok = file != INVALID_HANDLE_VALUE && WriteFile(file,&header,sizeof(header),&written,NULL) && written == sizeof(header) && WriteFile(file,&info.bmiHeader,sizeof(BITMAPINFOHEADER),&written,NULL) && written == sizeof(BITMAPINFOHEADER) && WriteFile(file,pixels,bytes,&written,NULL) && written == bytes;
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    SelectObject(dc,previous); DeleteObject(bitmap); DeleteDC(dc); return ok;
}
static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_CREATE: {
        app.window = window; app.dpi = GetDpiForWindow(window); app.split = 225; app.bottom = 180; app.sort_direction = 1; app.sort_column = 1;
        app.toolbar = control(TOOLBARCLASSNAMEW,WS_TABSTOP|TBSTYLE_TOOLTIPS|TBSTYLE_FLAT|TBSTYLE_LIST|CCS_NORESIZE|CCS_NOPARENTALIGN,0);
        SendMessageW(app.toolbar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);
        const int ids[] = {ID_SCAN,ID_STOP,ID_REFRESH,ID_SINGLE,ID_MONITOR,ID_EXPORT_MENU};
        TBBUTTON buttons[6] = {0}; for (int i = 0; i < 6; ++i) { buttons[i].iBitmap = I_IMAGENONE; buttons[i].idCommand = ids[i]; buttons[i].fsState = TBSTATE_ENABLED; buttons[i].fsStyle = (i == 4 ? BTNS_CHECK : i == 5 ? BTNS_DROPDOWN : BTNS_BUTTON)|BTNS_AUTOSIZE|BTNS_SHOWTEXT; buttons[i].iString = (INT_PTR)L" "; }
        SendMessageW(app.toolbar,TB_ADDBUTTONSW,6,(LPARAM)buttons);
        app.filter = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,window,(HMENU)ID_FILTER,GetModuleHandleW(NULL),NULL);
        SendMessageW(app.filter,EM_SETLIMITTEXT,127,0);
        app.summary = control(L"STATIC",SS_LEFT|SS_CENTERIMAGE,0);
        app.tree = control(WC_TREEVIEWW,WS_TABSTOP|TVS_HASLINES|TVS_HASBUTTONS|TVS_LINESATROOT|TVS_SHOWSELALWAYS,WS_EX_CLIENTEDGE);
        app.list = control(WC_LISTVIEWW,WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,WS_EX_CLIENTEDGE);
        ListView_SetExtendedListViewStyle(app.list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_HEADERDRAGDROP);
        for (int i = 0; i < 10; ++i) { wchar_t key[32]; swprintf_s(key,32,L"Column%d",i); int width = (int)GetPrivateProfileIntW(L"UI",key,widths[i],app.ini); if (width < 30 || width > 1500) width = widths[i]; LVCOLUMNW c = {0}; c.mask = LVCF_TEXT|LVCF_WIDTH; c.pszText = (wchar_t *)columns_en[i]; c.cx = px(width); ListView_InsertColumn(app.list,i,&c); }
        app.tabs = control(WC_TABCONTROLW,WS_TABSTOP,0);
        const wchar_t *tab_names[] = {L"General",L"Ports",L"Ping",L"History",L"Log",L"Statistics",L"Trace"};
        for (int i = 0; i < 7; ++i) { TCITEMW t = {0}; t.mask = TCIF_TEXT; t.pszText = (wchar_t *)tab_names[i]; TabCtrl_InsertItem(app.tabs,i,&t); }
        app.detail = control(L"EDIT",WS_TABSTOP|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,0);
        app.statistics_view = control(L"STATIC",SS_OWNERDRAW,0);
        app.log_view = control(WC_LISTVIEWW,WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,0);
        ListView_SetExtendedListViewStyle(app.log_view,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        const int log_widths[] = {145,190,125,400};
        for (int i = 0; i < 4; ++i) { LVCOLUMNW c = {0}; c.mask = LVCF_WIDTH|LVCF_TEXT; c.cx = px(log_widths[i]); c.pszText = L""; ListView_InsertColumn(app.log_view,i,&c); }
        app.status = control(STATUSCLASSNAMEW,SBARS_SIZEGRIP,0); app.progress = control(PROGRESS_CLASSW,0,0);
        if (!app.toolbar || !app.summary || !app.tree || !app.list || !app.tabs || !app.detail || !app.status || !app.progress || !app.filter || !app.log_view || !app.statistics_view) return -1;
        app.images = status_images(); ListView_SetImageList(app.list,app.images,LVSIL_SMALL);
        set_font(); localize(); enumerate(); SetTimer(window,1,50,NULL);
        if (app.smoke) SetTimer(window,2,250,NULL);
        return 0;
    }
    case WM_SIZE: layout(); return 0;
    case WM_DPICHANGED: {
        UINT old = app.dpi; app.dpi = HIWORD(w);
        for (int i = 0; i < 10; ++i) ListView_SetColumnWidth(app.list,i,MulDiv(ListView_GetColumnWidth(app.list,i),(int)app.dpi,(int)old));
        for (int i = 0; i < 4; ++i) ListView_SetColumnWidth(app.log_view,i,MulDiv(ListView_GetColumnWidth(app.log_view,i),(int)app.dpi,(int)old));
        HIMAGELIST images = status_images(); ListView_SetImageList(app.list,images,LVSIL_SMALL); if (app.images) ImageList_Destroy(app.images); app.images = images;
        set_font(); RECT *r = (RECT *)l; SetWindowPos(window,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE); localize(); return 0;
    }
    case WM_GETMINMAXINFO: ((MINMAXINFO *)l)->ptMinTrackSize.x = px(700); ((MINMAXINFO *)l)->ptMinTrackSize.y = px(430); return 0;
    case WM_TIMER: if (w == 2) {
        KillTimer(window,2);
        bool original = app.russian, original_system = app.system_language;
        if (!language_is_russian(L"system",MAKELANGID(LANG_RUSSIAN,SUBLANG_DEFAULT)) ||
            language_is_russian(L"system",MAKELANGID(LANG_GERMAN,SUBLANG_DEFAULT)) ||
            language_is_russian(L"",MAKELANGID(LANG_ENGLISH,SUBLANG_DEFAULT)) ||
            language_is_russian(L"en",MAKELANGID(LANG_RUSSIAN,SUBLANG_DEFAULT)) ||
            !language_is_russian(L"ru",MAKELANGID(LANG_ENGLISH,SUBLANG_DEFAULT))) ExitProcess(44);
        SendMessageW(window,WM_COMMAND,ID_EN,0);
        if (app.russian || app.system_language || !(GetMenuState(GetMenu(window),ID_EN,MF_BYCOMMAND)&MF_CHECKED)) ExitProcess(45);
        wchar_t text[128]; LVCOLUMNW column = {0}; column.mask = LVCF_TEXT; column.pszText = text; column.cchTextMax = 128;
        if (!ListView_GetColumn(app.list,1,&column) || wcscmp(text,L"IP Address")) ExitProcess(6);
        SendMessageW(window,WM_COMMAND,ID_RU,0);
        if (!app.russian || app.system_language) ExitProcess(46);
        if (!ListView_GetColumn(app.list,1,&column) || wcscmp(text,L"IP-адрес")) ExitProcess(7);
        SendMessageW(window,WM_COMMAND,ID_SYSTEM_LANGUAGE,0);
        if (!app.system_language || app.russian != language_is_russian(L"system",GetUserDefaultUILanguage())) ExitProcess(47);
        app.russian = original; app.system_language = original_system; localize();
        lan_host_t example = {0}; example.address = ipv4_address(0xc0a80101); example.online = true; example.has_mac = true;
        const uint8_t microsoft_mac[] = {0,0x50,0xf2,1,2,3}; memcpy(example.mac,microsoft_mac,6);
        memset(example.hostname,'n',253); example.hostname[253] = 0;
        ipv4_range_t scope = {0xc0a80101,0xc0a801fe}; monitor_begin(&app.model,scope,0,true,100);
        size_t index = monitor_observe(&app.model,&example,true,100); update_row(index); monitor_finish(&app.model,true,101); sync_log(false);
        wchar_t long_name[256]; ListView_GetItemText(app.list,0,2,long_name,256); if (wcslen(long_name) != 253) ExitProcess(29);
        if (!vendor_lookup(&app.vendors,microsoft_mac) || app.vendors.count < 1000) ExitProcess(10);
        SetWindowTextW(app.filter,L"192.168.1.1"); if (ListView_GetItemCount(app.list) != 1) ExitProcess(11);
        SetWindowTextW(app.filter,L"not-a-match"); if (ListView_GetItemCount(app.list) != 0) ExitProcess(12);
        SetWindowTextW(app.filter,L""); if (ListView_GetItemCount(app.list) != 1) ExitProcess(13);
        ListView_SetItemState(app.list,0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED); TabCtrl_SetCurSel(app.tabs,1); show_detail();
        if (TabCtrl_GetItemCount(app.tabs) != 7) ExitProcess(14);
        monitor_begin(&app.model,scope,0,true,102); monitor_finish(&app.model,true,103);
        monitor_begin(&app.model,scope,0,true,104); monitor_finish(&app.model,true,105); update_row(index);
        if (app.hosts[index].online) ExitProcess(15);
        monitor_begin(&app.model,scope,0,true,106); example.address = ipv4_address(0xc0a80102);
        size_t fresh = monitor_observe(&app.model,&example,true,106); update_row(fresh); monitor_finish(&app.model,true,107); sync_log(false);
        if (!app.hosts[fresh].new_device) ExitProcess(16);
        monitor_acknowledge(&app.model,fresh); update_row(fresh); if (app.hosts[fresh].new_device) ExitProcess(17);
        TabCtrl_SetCurSel(app.tabs,4); show_detail(); if (ListView_GetItemCount(app.log_view) != (int)app.model.log_count) ExitProcess(18);
        TabCtrl_SetCurSel(app.tabs,3); show_detail();
        TabCtrl_SetCurSel(app.tabs,5); show_detail();
        statistics_t stats = statistics_compute(app.hosts,app.host_count,0); if (stats.online != 1 || stats.offline != 1) ExitProcess(22);
        size_t detail_index = selected_host(); if (detail_index == SIZE_MAX) ExitProcess(31);
        uint32_t detail_ip = ipv4_value(&app.hosts[detail_index].address), detail_interface = app.hosts[detail_index].interface_index;
        if (!details_note_set(&app.details,detail_ip,detail_interface,"Session note")) ExitProcess(32);
        TabCtrl_SetCurSel(app.tabs,0); show_detail(); wchar_t detail_text[4096]; GetWindowTextW(app.detail,detail_text,4096); if (!wcsstr(detail_text,L"Session note")) ExitProcess(33);
        ping_sample_t ping_sample = {0}; ping_sample.ip = detail_ip; ping_sample.interface_index = detail_interface; ping_sample.timestamp = 123; ping_sample.latency_ms = 7; ping_sample.received = true;
        details_ping_add(&app.details,ping_sample); TabCtrl_SetCurSel(app.tabs,2); show_detail(); GetWindowTextW(app.detail,detail_text,4096); if (!wcsstr(detail_text,L"7 ms")) ExitProcess(34);
        app.trace_target = detail_ip; app.trace_interface = detail_interface; app.trace_count = 1; app.trace[0] = ping_sample; app.trace[0].hop = 1; app.trace[0].reply_ip = detail_ip;
        TabCtrl_SetCurSel(app.tabs,6); show_detail(); GetWindowTextW(app.detail,detail_text,4096); if (!wcsstr(detail_text,L"7 ms") || !(GetWindowLongPtrW(app.detail,GWL_STYLE)&WS_VISIBLE)) ExitProcess(35);
        TabCtrl_SetCurSel(app.tabs,5); show_detail();
        if (!win_profile_ui_smoke(window)) ExitProcess(23);
        DRAWITEMSTRUCT drawing = {0}; drawing.hwndItem = app.statistics_view; drawing.CtlType = ODT_STATIC; drawing.itemAction = ODA_DRAWENTIRE;
        GetClientRect(app.statistics_view,&drawing.rcItem); drawing.hDC = GetDC(app.statistics_view);
        if (!drawing.hDC) ExitProcess(24);
        SendMessageW(window,WM_DRAWITEM,0,(LPARAM)&drawing); ReleaseDC(app.statistics_view,drawing.hDC);
        if (app.selected >= 0) {
            app.has_monitor_range = true; app.monitor_range.first = app.monitor_range.last = app.interfaces[app.selected].ip;
            SendMessageW(window,WM_COMMAND,ID_MONITOR,0);
            if (!app.monitoring || !SendMessageW(app.toolbar,TB_ISBUTTONCHECKED,ID_MONITOR,0)) ExitProcess(19);
            SendMessageW(window,WM_COMMAND,ID_STOP,0);
            if (app.monitoring || SendMessageW(app.toolbar,TB_ISBUTTONCHECKED,ID_MONITOR,0)) ExitProcess(20);
            NMTREEVIEWW no_selection = {0}; no_selection.hdr.hwndFrom = app.tree; no_selection.hdr.code = TVN_SELCHANGEDW;
            SendMessageW(window,WM_NOTIFY,0,(LPARAM)&no_selection);
            if (app.selected != -1 || SendMessageW(app.toolbar,TB_ISBUTTONENABLED,ID_SCAN,0)) ExitProcess(21);
        }
        if (app.smoke_close_scan) {
            if (!app.interface_count) ExitProcess(77);
            app.selected = 0; reset_session(); bind_profile_interface();
            app.profile.ranges[0] = (ipv4_range_t){app.interfaces[0].ip,app.interfaces[0].ip}; app.profile.range_count = 1;
            if (!win_profile_scan_start(&app.backend,&app.interfaces[0],&app.profile,true,NULL,0)) ExitProcess(25);
            profile_scan_started();
        }
        if (app.smoke_close_tools) {
            if (!app.interface_count) ExitProcess(77);
            reset_session(); app.selected = 0;
            app.tools_job = win_tools_start(&app.backend.ping,app.interfaces[0].ip,app.interfaces[0].ip,app.interfaces[0].index,app.profile.icmp_timeout_ms,false);
            app.tools_session = app.session; app.tools_trace = false; if (!app.tools_job) ExitProcess(30);
        }
        if (app.smoke_dpi && !dpi_smoke()) ExitProcess(27);
        if (app.preview) {
            reset_session();
            app.selected = app.interface_count ? 0 : -1; update_status();
            ipv4_range_t range = {0xc0a80101,0xc0a801fe}; monitor_begin(&app.model,range,0,true,100);
            const char *names[] = {"gateway","workstation","nas"};
            for (unsigned i = 0; i < 3; ++i) { lan_host_t sample = {0}; sample.address = ipv4_address(0xc0a80101+i*9); sample.online = i != 2; sample.has_mac = true; memcpy(sample.mac,microsoft_mac,6); sample.mac[5] += (uint8_t)i; snprintf(sample.hostname,256,"%s",names[i]); sample.first_seen = sample.last_seen = (uint64_t)time(NULL); sample.has_latency = sample.online; sample.latency_ms = i+1; sample.os_family = i == 1 ? OS_WINDOWS : OS_UNKNOWN; sample.os_confidence = i == 1 ? 65 : 0; size_t row = monitor_observe(&app.model,&sample,sample.online,(uint64_t)time(NULL)); update_row(row); }
            SetWindowTextW(app.summary,tr(L"Preview — sample data, no network scan",L"Предпросмотр — демонстрационные данные, без сканирования"));
            app.scanning = true; app.started = GetTickCount64(); InterlockedExchange(&app.backend.cancel,0);
            update_status(); TabCtrl_SetCurSel(app.tabs,5); show_detail(); if (!preview_bitmap()) ExitProcess(28); app.scanning = false;
        }
        PostMessageW(window,WM_CLOSE,0,0);
    } else { tools_tick(); export_tick(); drain_events(); monitoring_tick(); } return 0;
    case WM_COMMAND:
        if (LOWORD(w) >= ID_INTERVAL_FIRST && LOWORD(w) < ID_INTERVAL_FIRST+5) {
            app.monitor_interval = intervals[LOWORD(w)-ID_INTERVAL_FIRST]; app.profile.monitor_interval_sec = app.monitor_interval; app.next_monitor = GetTickCount64()+(ULONGLONG)app.monitor_interval*1000; localize(); return 0;
        }
        if (LOWORD(w) == ID_FILTER && HIWORD(w) == EN_CHANGE) {
            GetWindowTextW(app.filter,app.filter_text,128); for (size_t i = 0; app.filter_text[i]; ++i) app.filter_text[i] = (wchar_t)towlower(app.filter_text[i]);
            SendMessageW(app.list,WM_SETREDRAW,FALSE,0); ListView_DeleteAllItems(app.list); for (size_t i = 0; i < app.host_count; ++i) update_row(i);
            ListView_SortItems(app.list,compare_rows,0); SendMessageW(app.list,WM_SETREDRAW,TRUE,0); InvalidateRect(app.list,NULL,FALSE); show_detail(); return 0;
        }
        switch (LOWORD(w)) {
        case ID_SCAN: begin_scan(); break;
        case ID_STOP: app.monitoring = false; win_scan_stop(&app.backend); win_tools_cancel(app.tools_job); update_summary(); update_status(); break;
        case ID_PING: start_tool(false); break;
        case ID_TRACE: start_tool(true); break;
        case ID_REFRESH: enumerate(); break;
        case ID_EN: app.system_language = false; app.russian = false; localize(); save_settings(); break;
        case ID_RU: app.system_language = false; app.russian = true; localize(); save_settings(); break;
        case ID_SYSTEM_LANGUAGE: app.system_language = true; app.russian = language_is_russian(L"system",GetUserDefaultUILanguage()); localize(); save_settings(); break;
        case ID_SINGLE: single_scan(false,false); break;
        case ID_PORTS: single_scan(true,false); break;
        case ID_CUSTOM: single_scan(true,true); break;
        case ID_RESCAN: single_scan(true,false); break;
        case ID_MONITOR: toggle_monitoring(); break;
        case ID_PROFILE_OPEN: case ID_PROFILE_SAVE: case ID_PROFILE_EDIT: case ID_PROFILE_NEW: profile_command(LOWORD(w)); break;
        case ID_EXPORT_CSV: case ID_EXPORT_TXT: case ID_EXPORT_HTML: export_hosts((report_format_t)(LOWORD(w)-ID_EXPORT_CSV)); break;
        case ID_EXPORT_MENU: export_menu(); break;
        case ID_STATISTICS: TabCtrl_SetCurSel(app.tabs,5); show_detail(); break;
        case ID_WEB: case ID_HTTPS: case ID_RDP: case ID_SMB: case ID_SSH: case ID_NOTES: case ID_COPY_IP: case ID_COPY_NAME: case ID_COPY_MAC: case ID_PROPERTIES: case ID_ACKNOWLEDGE: host_action(LOWORD(w)); break;
        case ID_ABOUT: MessageBoxW(window,tr(L"Look@LAN-ng " APP_VERSION_W L"\nNative portable LAN scanner\nC17 / Win32\n" APP_COPYRIGHT_W,L"Look@LAN-ng " APP_VERSION_W L"\nПортативный сканер локальной сети\nC17 / Win32\n" APP_COPYRIGHT_W),L"Look@LAN-ng",MB_OK); break;
        case ID_EXIT: PostMessageW(window,WM_CLOSE,0,0); break;
        } return 0;
    case WM_CONTEXTMENU: if ((HWND)w == app.list) { POINT point = {(short)LOWORD(l),(short)HIWORD(l)}; context_menu(point); return 0; } break;
    case WM_NOTIFY: {
        NMHDR *n = (NMHDR *)l;
        if (n->hwndFrom == app.tree && n->code == TVN_SELCHANGINGW && !app.loading_tree && (app.scanning || app.monitoring || app.closing)) return TRUE;
        if (n->hwndFrom == app.toolbar && n->code == NM_CUSTOMDRAW) {
            NMCUSTOMDRAW *draw = (NMCUSTOMDRAW *)l;
            if (draw->dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (draw->dwDrawStage == CDDS_ITEMPREPAINT) {
                wchar_t caption[128]; TBBUTTONINFOW info = {0}; info.cbSize = sizeof(info); info.dwMask = TBIF_TEXT|TBIF_STATE; info.pszText = caption; info.cchText = 128;
                if (SendMessageW(app.toolbar,TB_GETBUTTONINFOW,draw->dwItemSpec,(LPARAM)&info) < 0) return CDRF_DODEFAULT;
                int saved = SaveDC(draw->hdc); RECT rectangle = draw->rc;
                bool enabled = (info.fsState & TBSTATE_ENABLED) != 0;
                COLORREF background = GetSysColor(COLOR_BTNFACE);
                FillRect(draw->hdc,&rectangle,GetSysColorBrush(COLOR_BTNFACE));
                if (enabled && ((draw->uItemState & (CDIS_HOT|CDIS_SELECTED|CDIS_CHECKED)) || (info.fsState & TBSTATE_CHECKED))) {
                    background = RGB(220,234,241); HBRUSH highlight = CreateSolidBrush(background); FillRect(draw->hdc,&rectangle,highlight); DeleteObject(highlight);
                }
                const int icon_ids[] = {ID_SCAN,ID_STOP,ID_REFRESH,ID_SINGLE,ID_MONITOR,ID_EXPORT_MENU};
                for (unsigned i = 0; i < 6; ++i) if (draw->dwItemSpec == (DWORD_PTR)icon_ids[i]) win_toolbar_icon(draw->hdc,rectangle.left+px(8),rectangle.top+(rectangle.bottom-rectangle.top-px(20))/2,px(20),i,enabled,background);
                SelectObject(draw->hdc,app.font); SetBkMode(draw->hdc,TRANSPARENT);
                SetTextColor(draw->hdc,enabled ? GetSysColor(COLOR_BTNTEXT) : RGB(105,110,116));
                rectangle.left += px(34); rectangle.right -= px(draw->dwItemSpec == ID_EXPORT_MENU ? 24 : 10);
                DrawTextW(draw->hdc,caption,-1,&rectangle,DT_SINGLELINE|DT_VCENTER|DT_CENTER|DT_NOPREFIX);
                if (draw->dwItemSpec == ID_EXPORT_MENU) { RECT arrow = draw->rc; arrow.left = arrow.right-px(20); DrawTextW(draw->hdc,L"▾",1,&arrow,DT_SINGLELINE|DT_VCENTER|DT_CENTER); }
                if (enabled && (draw->uItemState&CDIS_FOCUS)) { RECT focus = draw->rc; InflateRect(&focus,-px(2),-px(2)); DrawFocusRect(draw->hdc,&focus); }
                RestoreDC(draw->hdc,saved); return CDRF_SKIPDEFAULT;
            }
        }
        if (n->hwndFrom == app.toolbar && n->code == TBN_DROPDOWN && ((NMTOOLBARW *)l)->iItem == ID_EXPORT_MENU) { export_menu(); return TBDDRET_DEFAULT; }
        if (n->hwndFrom == app.tree && n->code == TVN_SELCHANGEDW && !app.scanning && !app.monitoring) {
            if (app.loading_tree) return 0;
            NMTREEVIEWW *selection = (NMTREEVIEWW *)l;
            int selected = selection->itemNew.hItem ? (int)selection->itemNew.lParam : -1;
            if (selected <= -2) {
                if (selected >= -33 && (size_t)(-selected-2) < app.saved_count) load_profile_path(app.saved_profiles[-selected-2].path);
                else { app.loading_tree = true; TreeView_SelectItem(app.tree,app.selected >= 0 ? app.interface_items[app.selected] : NULL); app.loading_tree = false; }
                return 0;
            }
            if (selected >= 0 && (size_t)selected >= app.interface_count) selected = -1;
            if (selected != app.selected) {
                app.selected = selected; reset_session(); bind_profile_interface(); update_summary(); update_status(); show_detail();
            }
        }
        if (n->hwndFrom == app.list && (n->code == LVN_ITEMCHANGED || n->code == NM_DBLCLK)) show_detail();
        if (n->hwndFrom == app.tabs && n->code == TCN_SELCHANGE) show_detail();
        if (n->hwndFrom == app.list && n->code == LVN_COLUMNCLICK) { int col = ((NMLISTVIEW *)l)->iSubItem; app.sort_direction = app.sort_column == col ? -app.sort_direction : 1; app.sort_column = col; ListView_SortItems(app.list,compare_rows,0); }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        RECT r; GetClientRect(window,&r); int x = (short)LOWORD(l), y = (short)HIWORD(l);
        if (abs(x-px(app.split)) < px(8)) app.drag = 1;
        else if (x > px(app.split) && abs(y-(r.bottom-px(app.bottom)-px(24))) < px(12)) app.drag = 2;
        if (app.drag) SetCapture(window); return 0;
    }
    case WM_MOUSEMOVE: if (app.drag) { RECT r; GetClientRect(window,&r); if (app.drag == 1) app.split = MulDiv((short)LOWORD(l),96,(int)app.dpi); else app.bottom = MulDiv(r.bottom-(short)HIWORD(l)-px(24),96,(int)app.dpi); if (app.split < 130) app.split = 130; if (app.bottom < 100) app.bottom = 100; layout(); } return 0;
    case WM_LBUTTONUP: app.drag = 0; ReleaseCapture(); return 0;
    case WM_DRAWITEM: if (((DRAWITEMSTRUCT *)l)->hwndItem == app.statistics_view) { paint_statistics((DRAWITEMSTRUCT *)l); return TRUE; } break;
    case WM_CLOSE: if (app.closing) return 0; app.closing = true; app.monitoring = false; win_tools_cancel(app.tools_job); win_backend_shutdown(&app.backend); win_export_cancel(app.export_job); update_status(); if (!app.tools_job && !app.export_job && win_backend_release(&app.backend)) DestroyWindow(window); return 0;
    case WM_DESTROY: save_settings(); KillTimer(window,1); if (app.font) DeleteObject(app.font); if (app.images) ImageList_Destroy(app.images); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command, int show) {
    (void)previous;
    app.smoke_close_scan = wcsstr(command,L"--smoke-close-scan") != NULL;
    app.smoke_close_tools = wcsstr(command,L"--smoke-close-tools") != NULL;
    app.smoke_dpi = wcsstr(command,L"--smoke-dpi") != NULL;
    app.preview = wcsstr(command,L"--smoke-preview") != NULL;
    app.smoke = wcsstr(command,L"--smoke-test") != NULL || app.smoke_close_scan || app.smoke_close_tools || app.smoke_dpi || app.preview;
    settings_path(); wchar_t language[8]; GetPrivateProfileStringW(L"UI",L"Language",L"",language,8,app.ini);
    app.system_language = wcscmp(language,L"ru") != 0 && wcscmp(language,L"en") != 0;
    app.russian = language_is_russian(language,GetUserDefaultUILanguage());
    if (app.preview && wcsstr(command,L"--smoke-preview-en")) app.russian = false;
    app.monitor_interval = 30;
    unsigned saved_interval = GetPrivateProfileIntW(L"UI",L"MonitorInterval",30,app.ini);
    for (unsigned i = 0; i < 5; ++i) if (intervals[i] == saved_interval) app.monitor_interval = saved_interval;
    profile_defaults(&app.profile); app.profile.monitor_interval_sec = app.monitor_interval;
    monitor_init(&app.model,app.hosts,&app.host_count,HOST_CAPACITY,app.log,LOG_CAPACITY);
    WSADATA data; if (WSAStartup(MAKEWORD(2,2),&data)) return 1;
    if (!win_backend_init(&app.backend)) return 2;
    HRSRC vendor_resource = FindResourceW(instance,MAKEINTRESOURCEW(2),RT_RCDATA);
    if (!vendor_resource) return 8;
    HGLOBAL vendor_data = LoadResource(instance,vendor_resource);
    if (!vendor_data || !vendor_open(&app.vendors,LockResource(vendor_data),SizeofResource(instance,vendor_resource))) return 9;
    INITCOMMONCONTROLSEX common = {sizeof(common),ICC_WIN95_CLASSES|ICC_PROGRESS_CLASS}; InitCommonControlsEx(&common);
    WNDCLASSW cls = {0}; cls.lpfnWndProc = window_proc; cls.hInstance = instance; cls.lpszClassName = L"LookAtLANngWindow"; cls.hCursor = LoadCursorW(NULL,IDC_ARROW); cls.hIcon = LoadIconW(instance,MAKEINTRESOURCEW(1)); cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    if (!RegisterClassW(&cls)) return 3;
    HWND window = CreateWindowExW(0,cls.lpszClassName,L"Look@LAN-ng " APP_VERSION_W,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1100,700,NULL,NULL,instance,NULL);
    if (!window) return 4;
    ShowWindow(window,app.smoke ? SW_HIDE : show);
    ACCEL shortcuts[] = {{FVIRTKEY,VK_F5,ID_SCAN},{FVIRTKEY,VK_ESCAPE,ID_STOP},{FVIRTKEY|FCONTROL,'O',ID_PROFILE_OPEN},{FVIRTKEY|FCONTROL,'S',ID_PROFILE_SAVE}}; HACCEL accel = CreateAcceleratorTableW(shortcuts,4);
    MSG message; int result;
    while ((result = GetMessageW(&message,NULL,0,0)) > 0) {
        if (!TranslateAcceleratorW(window,accel,&message) && !IsDialogMessageW(window,&message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    DestroyAcceleratorTable(accel);
    if (app.smoke && app.backend.initialized) ExitProcess(26);
    WSACleanup();
    ExitProcess(result < 0 ? 5 : 0);
}
