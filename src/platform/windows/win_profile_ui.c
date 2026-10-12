#include "win_profile_ui.h"
#include "win_files.h"
#include "ports.h"
#include "../../resources/resource.h"
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct { bool russian; profile_t *candidate; } editor_t;
static const wchar_t *lang(editor_t *e, const wchar_t *en, const wchar_t *ru) { return e->russian ? ru : en; }
static void set_utf8(HWND dialog, int control, const char *text) {
    wchar_t wide[8192]; if (!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,wide,8192)) wide[0] = 0; SetDlgItemTextW(dialog,control,wide);
}
static bool get_utf8(HWND dialog, int control, char *text, size_t capacity) {
    wchar_t wide[8192]; GetDlgItemTextW(dialog,control,wide,8192);
    return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide,-1,text,(int)capacity,NULL,NULL) != 0;
}
static bool read_number(HWND dialog, int control, uint32_t *value) {
    wchar_t text[32]; int n = GetDlgItemTextW(dialog,control,text,32); uint32_t v = 0;
    if (!n || n > 10) return false;
    for (int i = 0; i < n; ++i) { if (text[i] < L'0' || text[i] > L'9') return false; unsigned d = (unsigned)(text[i]-L'0'); if (v > (UINT32_MAX-d)/10) return false; v = v*10+d; }
    *value = v; return true;
}
static bool read_editor(HWND dialog, editor_t *editor) {
    profile_t *p = editor->candidate;
    if (!get_utf8(dialog,IDC_PROFILE_NAME,p->name,sizeof(p->name)) || !get_utf8(dialog,IDC_PROFILE_NOTES,p->notes,sizeof(p->notes))) return false;
    char ranges[8192], ports[8192]; if (!get_utf8(dialog,IDC_PROFILE_RANGES,ranges,sizeof(ranges)) || !get_utf8(dialog,IDC_PROFILE_PORTS,ports,sizeof(ports))) return false;
    p->range_count = 0; size_t length = strlen(ranges), pos = 0;
    while (pos < length) {
        size_t first = pos; while (pos < length && ranges[pos] != '\n') ++pos; size_t end = pos; if (pos < length) ++pos; if (end > first && ranges[end-1] == '\r') --end;
        while (first < end && (ranges[first] == ' ' || ranges[first] == '\t')) ++first;
        while (end > first && (ranges[end-1] == ' ' || ranges[end-1] == '\t')) --end;
        if (first == end) continue;
        if (p->range_count == NETWORK_MAX_RANGES || !range_parse(ranges+first,end-first,&p->ranges[p->range_count])) return false;
        ++p->range_count;
    }
    uint64_t total;
    if (!ranges_normalize(p->ranges,&p->range_count,&total) || !ports_parse(ports,strlen(ports),p->ports,65535,&p->port_count)) return false;
    if (!read_number(dialog,IDC_PROFILE_ICMP,&p->icmp_timeout_ms) || !read_number(dialog,IDC_PROFILE_TCP,&p->tcp_timeout_ms) || !read_number(dialog,IDC_PROFILE_CONCURRENCY,&p->concurrency)) return false;
    int selected = (int)SendDlgItemMessageW(dialog,IDC_PROFILE_INTERVAL,CB_GETCURSEL,0,0); const unsigned intervals[] = {5,10,30,60,300}; if (selected < 0 || selected >= 5) return false;
    p->monitor_interval_sec = intervals[selected]; return profile_validate(p,NULL,0);
}
static INT_PTR CALLBACK editor_proc(HWND dialog, UINT message, WPARAM w, LPARAM l) {
    editor_t *e = (editor_t *)GetWindowLongPtrW(dialog,DWLP_USER);
    if (message == WM_INITDIALOG) {
        e = (editor_t *)l; SetWindowLongPtrW(dialog,DWLP_USER,l); SetWindowTextW(dialog,lang(e,L"Scan profile",L"Профиль сканирования"));
        const wchar_t *en[] = {L"Name",L"IPv4 ranges",L"TCP ports",L"ICMP timeout, ms",L"TCP timeout, ms",L"Concurrency",L"Monitoring, s",L"Notes",L"One range per line: 192.168.1.1-254, 10.0.0.0/24"};
        const wchar_t *ru[] = {L"Имя",L"Диапазоны IPv4",L"TCP-порты",L"ICMP таймаут, мс",L"TCP таймаут, мс",L"Параллельные пробы",L"Мониторинг, с",L"Заметки",L"Один диапазон в строке: 192.168.1.1-254, 10.0.0.0/24"};
        for (int i = 0; i < 9; ++i) SetDlgItemTextW(dialog,230+i,e->russian ? ru[i] : en[i]);
        SetDlgItemTextW(dialog,IDCANCEL,lang(e,L"Cancel",L"Отмена"));
        char ranges[8192], ports[8192]; profile_ranges_text(e->candidate,ranges,sizeof(ranges)); profile_ports_text(e->candidate,ports,sizeof(ports));
        set_utf8(dialog,IDC_PROFILE_NAME,e->candidate->name); set_utf8(dialog,IDC_PROFILE_RANGES,ranges); set_utf8(dialog,IDC_PROFILE_PORTS,ports); set_utf8(dialog,IDC_PROFILE_NOTES,e->candidate->notes);
        SetDlgItemInt(dialog,IDC_PROFILE_ICMP,e->candidate->icmp_timeout_ms,FALSE); SetDlgItemInt(dialog,IDC_PROFILE_TCP,e->candidate->tcp_timeout_ms,FALSE); SetDlgItemInt(dialog,IDC_PROFILE_CONCURRENCY,e->candidate->concurrency,FALSE);
        const unsigned intervals[] = {5,10,30,60,300}; for (unsigned i = 0; i < 5; ++i) { wchar_t text[16]; swprintf_s(text,16,L"%u",intervals[i]); SendDlgItemMessageW(dialog,IDC_PROFILE_INTERVAL,CB_ADDSTRING,0,(LPARAM)text); if (intervals[i] == e->candidate->monitor_interval_sec) SendDlgItemMessageW(dialog,IDC_PROFILE_INTERVAL,CB_SETCURSEL,i,0); }
        SendDlgItemMessageW(dialog,IDC_PROFILE_NAME,EM_SETLIMITTEXT,255,0); SendDlgItemMessageW(dialog,IDC_PROFILE_RANGES,EM_SETLIMITTEXT,4096,0); SendDlgItemMessageW(dialog,IDC_PROFILE_PORTS,EM_SETLIMITTEXT,4096,0); SendDlgItemMessageW(dialog,IDC_PROFILE_NOTES,EM_SETLIMITTEXT,2047,0);
        return TRUE;
    }
    if (message == WM_COMMAND && LOWORD(w) == IDOK && e) {
        if (read_editor(dialog,e)) EndDialog(dialog,IDOK);
        else MessageBoxW(dialog,lang(e,L"Check ranges/ports and text length. ICMP: 50–5000 ms; TCP: 25–5000 ms; concurrency: 8–256.",L"Проверьте диапазоны/порты и длину текста. ICMP: 50–5000 мс; TCP: 25–5000 мс; параллельные пробы: 8–256."),L"Look@LAN-ng",MB_ICONWARNING);
        return TRUE;
    }
    if ((message == WM_COMMAND && LOWORD(w) == IDCANCEL) || message == WM_CLOSE) { EndDialog(dialog,IDCANCEL); return TRUE; }
    return FALSE;
}
bool win_profile_edit(HWND owner, bool ru, profile_t *profile) {
    profile_t *candidate = malloc(sizeof(*candidate)); if (!candidate) return false; *candidate = *profile; editor_t editor = {ru,candidate};
    bool accepted = DialogBoxParamW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(IDD_PROFILE),owner,editor_proc,(LPARAM)&editor) == IDOK;
    if (accepted) *profile = *candidate; free(candidate); return accepted;
}
bool win_profile_ui_smoke(HWND owner) {
    profile_t *candidate = malloc(sizeof(*candidate)); if (!candidate) return false;
    bool ok = true;
    for (unsigned language = 0; language < 2 && ok; ++language) {
        profile_defaults(candidate); candidate->ranges[0] = (ipv4_range_t){0xc0a80101,0xc0a801fe}; candidate->range_count = 1;
        editor_t editor = {language != 0,candidate};
        HWND dialog = CreateDialogParamW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(IDD_PROFILE),owner,editor_proc,(LPARAM)&editor);
        if (!dialog) { ok = false; break; }
        SetDlgItemTextW(dialog,IDC_PROFILE_NAME,L"Офис"); SetDlgItemTextW(dialog,IDC_PROFILE_RANGES,L"192.168.1.1-254\r\n10.0.0.1-20");
        ok = read_editor(dialog,&editor) && candidate->range_count == 2 && candidate->ranges[0].first == 0x0a000001 && !strcmp(candidate->name,"Офис");
        SetDlgItemTextW(dialog,IDC_PROFILE_RANGES,L"10.0.0.5-1"); if (read_editor(dialog,&editor)) ok = false;
        DestroyWindow(dialog);
    }
    free(candidate); return ok;
}
bool win_profile_file_dialog(HWND owner, bool save, bool ru, wchar_t *path) {
    wchar_t directory[MAX_PATH] = L""; win_profile_directory(directory,MAX_PATH,save);
    OPENFILENAMEW ofn = {0}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner; ofn.lpstrFile = path; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = ru ? L"Профили Look@LAN-ng\0*.lanprofile\0Все файлы\0*.*\0\0" : L"Look@LAN-ng profiles\0*.lanprofile\0All files\0*.*\0\0";
    ofn.lpstrDefExt = L"lanprofile"; ofn.lpstrInitialDir = directory; ofn.Flags = OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    ofn.lpstrTitle = ru ? (save ? L"Сохранить профиль" : L"Открыть профиль") : (save ? L"Save profile" : L"Open profile");
    return save ? GetSaveFileNameW(&ofn) != 0 : GetOpenFileNameW(&ofn) != 0;
}
bool win_report_file_dialog(HWND owner, report_format_t format, bool ru, wchar_t *path) {
    const wchar_t *filters[] = {L"CSV\0*.csv\0\0",L"Text\0*.txt\0\0",L"HTML\0*.html\0\0"}; const wchar_t *extensions[] = {L"csv",L"txt",L"html"};
    if (format > REPORT_HTML) return false;
    OPENFILENAMEW ofn = {0}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = owner; ofn.lpstrFile = path; ofn.nMaxFile = MAX_PATH; ofn.lpstrFilter = filters[format]; ofn.lpstrDefExt = extensions[format];
    ofn.lpstrTitle = ru ? L"Экспорт всех узлов сессии" : L"Export all session hosts"; ofn.Flags = OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
    return GetSaveFileNameW(&ofn) != 0;
}
