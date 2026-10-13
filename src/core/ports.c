#include "ports.h"
#include <string.h>
const uint16_t discovery_ports[] = {22,23,53,80,135,139,443,445,3389,5985,5986,8080,8443};
const size_t discovery_port_count = sizeof(discovery_ports)/sizeof(discovery_ports[0]);
const uint16_t quick_ports[] = {21,22,23,25,53,80,110,135,139,143,443,445,587,993,995,1433,3306,3389,5432,5900,5985,5986,8080,8443};
const size_t quick_port_count = sizeof(quick_ports)/sizeof(quick_ports[0]);
static void spaces(const char *s, size_t n, size_t *p) { while (*p < n && (s[*p]==' ' || s[*p]=='\t')) ++*p; }
static bool number(const char *s, size_t n, size_t *p, unsigned *v) {
    unsigned value = 0, digits = 0;
    spaces(s,n,p);
    while (*p < n && s[*p] >= '0' && s[*p] <= '9') {
        value = value*10+(unsigned)(s[(*p)++]-'0');
        if (++digits > 5 || value > 65535) return false;
    }
    if (!digits || !value) return false;
    *v = value; spaces(s,n,p); return true;
}
bool ports_parse(const char *s, size_t n, uint16_t *out, size_t cap, size_t *count) {
    if (!s || !out || !count || !n || n > 65535) return false;
    uint8_t seen[8192] = {0}; size_t pos = 0, total = 0;
    while (pos < n) {
        unsigned first, last;
        if (!number(s,n,&pos,&first)) return false;
        last = first;
        if (pos < n && s[pos]=='-') { ++pos; if (!number(s,n,&pos,&last) || last < first) return false; }
        for (unsigned p = first; p <= last; ++p) {
            uint8_t bit = (uint8_t)(1u << (p & 7));
            if (!(seen[p >> 3] & bit)) { seen[p >> 3] |= bit; if (++total > cap) return false; }
        }
        if (pos == n) break;
        if (s[pos++] != ',' || pos == n) return false;
    }
    size_t at = 0;
    for (unsigned p = 1; p <= 65535; ++p) if (seen[p >> 3] & (1u << (p & 7))) out[at++] = (uint16_t)p;
    *count = at; return at != 0;
}
const char *port_service(uint16_t p) {
    switch (p) {
    case 21:return "FTP"; case 22:return "SSH"; case 23:return "Telnet"; case 25:return "SMTP"; case 53:return "DNS";
    case 80:case 8080:return "HTTP"; case 110:return "POP3"; case 135:return "RPC"; case 139:return "NetBIOS";
    case 143:return "IMAP"; case 443:case 8443:return "HTTPS"; case 445:return "SMB"; case 587:return "SMTP";
    case 993:return "IMAPS"; case 995:return "POP3S"; case 1433:return "MSSQL"; case 3306:return "MySQL";
    case 3389:return "RDP"; case 5432:return "PostgreSQL"; case 5900:return "VNC"; case 5985:return "WinRM";
    case 5986:return "WinRM HTTPS"; case 9100:return "Printer"; default:return "TCP";
    }
}
