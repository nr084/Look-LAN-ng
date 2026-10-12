# Техническое задание v1.00
## Современный минималистичный LAN Scanner / Network Monitor

Рабочее название проекта: **LANScope**.

Название временное. Не использовать название, логотип, иконки, графические ресурсы или исходный код старого Look@LAN. Разрешено воспроизвести его общую концепцию, workflow и удобство интерфейса.

---

# 1. Цель

Создать современный аналог классического Look@LAN:

- очень простой;
- очень быстрый;
- маленький portable EXE;
- без установки;
- без облака;
- без аккаунтов;
- без драйверов;
- без фоновых сервисов;
- без .NET;
- без Java;
- без Electron;
- без WebView;
- без OpenGL;
- без DirectX-рендеринга интерфейса;
- без Npcap/WinPcap;
- без внешнего runtime.

Первый релиз:

**Windows 10 / Windows 11 x64.**

В дальнейшем:

- Linux;
- macOS.

Поэтому network scanner core должен изначально проектироваться переносимым и не зависеть напрямую от Win32.

Windows-специфичные части должны находиться только в platform/windows.

---

# 2. Главный принцип продукта

Программа должна следовать философии старых системных утилит:

> Download → Run → Scan.

Пользователь запускает один EXE и через несколько секунд видит устройства своей локальной сети.

Не превращать программу в enterprise network management system.

Не добавлять:

- web server;
- backend;
- agent;
- cloud sync;
- authentication;
- telemetry;
- user accounts;
- online dashboard;
- plugin framework;
- scripting engine;
- встроенный браузер.

---

# 3. Основной стек

## Язык

**C17.**

Основной код проекта пишется на чистом C.

Не использовать C++ без абсолютно необходимой причины.

---

# 4. Windows UI

Использовать исключительно native Win32:

- USER32;
- GDI/GDI+ только если действительно понадобится;
- COMCTL32;
- SHELL32;
- native Windows common controls.

Основные элементы:

- TreeView;
- ListView;
- Tab Control;
- Toolbar;
- StatusBar;
- Splitter;
- standard dialogs;
- native menus.

Не создавать собственный GPU renderer.

Не использовать:

- OpenGL;
- Vulkan;
- Direct2D как основной UI framework;
- Direct3D;
- SDL;
- Qt;
- GTK на Windows;
- wxWidgets;
- Dear ImGui.

Интерфейс должен выглядеть как качественная классическая Windows desktop utility.

---

# 5. Внешний вид

По структуре интерфейс должен напоминать классический Look@LAN.

Не делать современный интерфейс из огромных карточек и пустого пространства.

Главное окно:

```text
+----------------------------------------------------------------+
| File  Scan  View  Tools  Help                                  |
+----------------------------------------------------------------+
| Scan | Stop | Refresh | Add Range | Single Host | Export       |
+--------------------+-------------------------------------------+
| NETWORKS           | Hosts                                     |
|                    |                                           |
| > Local Network    | ● 192.168.1.1   router      MikroTik      |
|   192.168.1.0/24   | ● 192.168.1.10  SERVER01    Windows       |
|                    | ● 192.168.1.20  NAS01       Synology      |
| > Saved Profiles   | ○ 192.168.1.30  PC01        Offline       |
|                    |                                           |
+--------------------+-------------------------------------------+
|                    | General | Ports | Ping | Trace | History   |
+--------------------+-------------------------------------------+
| Online: 18 | Offline: 4 | Scanned: 254/254 | 00:03.1          |
+----------------------------------------------------------------+
```

Использовать компактные размеры элементов.

Поддерживать:

- Windows DPI scaling;
- Per-Monitor DPI awareness v2;
- 100/125/150/175/200%;
- системные шрифты Windows;
- keyboard navigation;
- resizing;
- сохранение размеров колонок.

Начальный интерфейс — светлый native Windows.

Dark Mode не является задачей первого релиза.

---

# 6. Главное представление устройств

Основной ListView должен иметь сортируемые колонки:

- Status
- IP Address
- Host Name
- MAC Address
- Vendor
- Device / OS
- Latency
- Services
- First Seen
- Last Seen

Status:

- ONLINE
- OFFLINE
- NEW
- CHANGED
- UNKNOWN

Для ONLINE использовать маленький зелёный индикатор.

Для OFFLINE — серый.

Для NEW — отдельная отметка.

Не делать большие цветные строки.

---

# 7. Автоматическое обнаружение сети

При запуске программа должна определить:

- активные network adapters;
- IPv4 address;
- subnet mask / prefix;
- default gateway;
- DNS servers;
- interface metric;
- interface name;
- MAC локального интерфейса.

Не сканировать автоматически:

- loopback;
- disconnected adapters;
- VPN/tunnel interfaces без явного выбора;
- APIPA 169.254.0.0/16;
- виртуальные интерфейсы, если пользователь их не выбрал.

Пользователь должен видеть обнаруженную сеть, например:

```text
Ethernet
192.168.1.54
255.255.255.0
Network: 192.168.1.0/24
Gateway: 192.168.1.1

[ Scan ]
```

Если найдено несколько физических сетевых интерфейсов — показать их в дереве.

---

# 8. Определение диапазона

Для обычной /24 сети программа автоматически предлагает:

```text
192.168.1.1 - 192.168.1.254
```

Поддержать ручное задание:

```text
192.168.1.1-254
192.168.10.1-192.168.10.200
10.0.0.0/24
```

Поддержать несколько scan ranges в одном profile.

Защититься от случайного огромного сканирования.

Если диапазон содержит более 4096 адресов — запросить подтверждение.

---

# 9. Discovery pipeline

Сканирование не должно основываться только на ICMP.

Многие устройства запрещают ping, но доступны через TCP.

Использовать несколько методов.

## Stage 1 — existing neighbor cache

Прочитать системную neighbor/ARP table.

Полученные устройства сразу показывать пользователю.

UI должен наполняться постепенно, а не ждать окончания полного сканирования.

---

# 10. Stage 2 — ARP discovery

Для устройств в directly connected IPv4 subnet использовать ARP resolution.

ARP является главным методом быстрого поиска устройств внутри одного L2 segment.

Получать:

- IPv4;
- MAC.

Не требовать Administrator.

---

# 11. Stage 3 — ICMP

Для каждого адреса использовать ICMP echo.

Сохранять:

- alive/not alive;
- RTT;
- TTL если доступен.

ICMP timeout configurable.

Default:

```text
300 ms
```

Не считать устройство offline только потому, что ICMP заблокирован.

---

# 12. Stage 4 — TCP discovery

Для адресов, не ответивших на ICMP, выполнять быстрые TCP connect probes.

Default discovery ports:

```text
22
23
53
80
135
139
443
445
3389
5985
5986
8080
8443
```

Если TCP connection:

- succeeded;
- refused;

устройство считается существующим.

Timeout должен быть коротким.

Не выполнять полный port scan автоматически.

---

# 13. Hostname discovery

Использовать несколько источников.

Приоритет:

1. Reverse DNS.
2. NetBIOS name query.
3. mDNS — в последующих версиях.
4. локальный cache.

Если имя не найдено:

```text
<unknown>
```

Discovery hostname не должен блокировать основной scan.

Все hostname resolution выполнять асинхронно.

---

# 14. MAC Vendor

По MAC показывать производителя:

```text
D8:47:32:xx:xx:xx
MikroTik
```

Использовать IEEE OUI database.

Не включать огромный текстовый CSV напрямую.

При сборке должен существовать generator, который превращает исходную базу vendor prefixes в компактный бинарный lookup table.

Таблица может быть встроена в EXE как resource.

Lookup:

```text
MAC prefix -> vendor
```

База производителя не должна значительно замедлять запуск программы.

---

# 15. Device / OS detection

Не обещать точное fingerprinting уровня Nmap.

Использовать только безопасные признаки.

Например:

- TTL hints;
- открытые TCP ports;
- SMB availability;
- RDP;
- WinRM;
- SSH;
- HTTP banners, если они доступны без сложного protocol parser;
- MAC vendor;
- NetBIOS;
- mDNS позже.

Результат должен иметь confidence.

Например:

```text
Windows — probable
Linux/Unix — probable
Router / embedded — probable
Printer — probable
NAS — probable
Unknown
```

Не показывать:

```text
Windows Server 2025
```

если программа этого достоверно не определила.

Внутренняя структура:

```c
typedef enum {
    OS_UNKNOWN,
    OS_WINDOWS,
    OS_LINUX,
    OS_MACOS,
    OS_BSD,
    OS_NETWORK_DEVICE,
    OS_PRINTER,
    OS_NAS,
    OS_EMBEDDED
} os_family_t;
```

Хранить confidence отдельно.

---

# 16. Quick port scan

Для выбранного устройства пользователь может нажать:

```text
Scan Ports
```

Quick Scan:

```text
21 FTP
22 SSH
23 Telnet
25 SMTP
53 DNS
80 HTTP
110 POP3
135 RPC
139 NetBIOS
143 IMAP
443 HTTPS
445 SMB
587 SMTP
993 IMAPS
995 POP3S
1433 MSSQL
3306 MySQL
3389 RDP
5432 PostgreSQL
5900 VNC
5985 WinRM
5986 WinRM HTTPS
8080 HTTP
8443 HTTPS
```

Также:

```text
Custom ports...
```

Поддержать диапазон:

```text
1-1024
80,443,8080
20-25,80,443
```

Полный scan 1-65535 должен запускаться только вручную.

---

# 17. Device details

Double-click по устройству открывает нижний detail panel.

Tabs:

## General

Показывать:

- status;
- IP;
- hostname;
- MAC;
- vendor;
- OS/device guess;
- latency;
- first seen;
- last seen;
- detected interface;
- notes.

## Ports

Открытые обнаруженные порты.

## Ping

Последние результаты ping.

## Trace

Traceroute по запросу пользователя.

## History

Изменения состояния устройства в текущем monitoring session.

---

# 18. Быстрые действия

Context menu устройства:

```text
Ping
Traceroute
Scan Ports
Open Web
Open HTTPS
Open RDP
Open SSH
Open \\HOST
Copy IP
Copy Hostname
Copy MAC
Rescan Host
Properties
```

Open RDP:

```text
mstsc.exe /v:<host>
```

Open SSH:

если Windows OpenSSH client доступен:

```text
ssh.exe user@host
```

Но не требовать SSH как dependency.

Open SMB:

```text
\\host
```

HTTP/HTTPS — системный default browser.

---

# 19. Single Host Scan

Отдельная функция как в классических LAN tools:

```text
Scan -> Single Host
```

Пользователь вводит:

```text
192.168.1.10
```

или:

```text
server01
```

Программа выполняет расширенное исследование одного узла.

---

# 20. Monitoring mode

После первоначального scan пользователь может включить:

```text
Monitoring
```

Программа периодически проверяет найденные устройства.

Default interval:

```text
30 seconds
```

В настройках:

```text
5 sec
10 sec
30 sec
60 sec
5 min
```

События:

```text
HOST ONLINE
HOST OFFLINE
NEW HOST
IP CHANGED
MAC CHANGED
HOSTNAME CHANGED
```

Сканирование не должно постоянно создавать высокий network load.

---

# 21. NEW devices

Очень важная возможность.

Если во время Monitoring появляется ранее неизвестный MAC/IP:

```text
NEW DEVICE

192.168.1.87
F8:xx:xx:xx:xx:xx
Xiaomi
```

Пометить устройство как NEW.

Не показывать Windows toast notifications в первом MVP.

Достаточно визуальной отметки и Network Log.

---

# 22. Network Log

В нижней панели сделать tab:

```text
Log
```

Пример:

```text
10:21:03  Scan started        192.168.1.0/24
10:21:03  ONLINE              192.168.1.1
10:21:04  ONLINE              192.168.1.10 SERVER01
10:21:05  NEW DEVICE          192.168.1.87
10:22:42  OFFLINE             192.168.1.20 NAS01
10:23:05  ONLINE              192.168.1.20 NAS01
```

Log должен быть ограничен по памяти.

Например:

```text
max 10 000 entries
```

---

# 23. Statistics

Не делать сложный dashboard.

Небольшая Statistics page:

```text
Addresses scanned: 254
Online:            21
Offline:           233
New:               2

Windows:           8
Linux/Unix:        4
Network devices:   5
Unknown:           4
```

При необходимости нарисовать маленькие графики обычным GDI.

Никакого GPU renderer.

---

# 24. Network Tree

Левая панель:

```text
Networks
 ├─ Ethernet
 │   └─ 192.168.1.0/24
 │
 ├─ Wi-Fi
 │   └─ 192.168.50.0/24
 │
 └─ Saved Profiles
     ├─ Office
     ├─ Home
     └─ Customer A
```

TreeView должен использовать native Windows control.

---

# 25. Profiles

Пользователь может сохранить:

- scan ranges;
- selected interface;
- port list;
- timeouts;
- monitoring interval;
- notes.

Не использовать database.

Profile хранить в небольшом versioned UTF-8 text format.

Например:

```text
LANScopeProfile=1

name=Office
range=192.168.1.1-254
range=192.168.2.1-254

icmp_timeout_ms=300
tcp_timeout_ms=250
monitor_interval_sec=30
```

Parser сделать внутри portable core.

---

# 26. Export

Поддержать:

```text
CSV
TXT
HTML
```

HTML report должен быть автономным.

Не использовать JavaScript.

Пример таблицы:

```text
Status | IP | Hostname | MAC | Vendor | Device | Latency | Services
```

---

# 27. Portable mode

Главная поставка:

```text
LANScope.exe
```

Один EXE.

Не требовать installation.

Не создавать Windows Service.

Не добавлять приложение в startup.

Не изменять firewall.

Не писать в registry без необходимости.

Если пользователь ничего не сохраняет — приложение должно оставлять минимум следов.

Настройки можно сохранять в:

```text
LANScope.ini
```

рядом с EXE, если directory writable.

Если directory read-only:

```text
%LOCALAPPDATA%\LANScope\
```

Но сам scanner обязан работать даже если настройки сохранить нельзя.

---

# 28. Размер

Приоритет — маленький бинарник.

Target:

```text
LANScope.exe <= 2 MB
```

Желательно.

Допустимо:

```text
<= 5 MB
```

если внутрь включена полноценная MAC vendor database.

Не добавлять библиотеку только ради небольшой удобной функции, которую можно безопасно реализовать небольшим количеством C-кода.

---

# 29. Build

Primary compiler:

```text
MSVC v143
```

Architecture:

```text
x64
```

Build system:

```text
CMake
Ninja
```

Release:

```text
C17
/MT
/O2
/GL
/LTCG
```

Security:

```text
/NXCOMPAT
/DYNAMICBASE
/HIGHENTROPYVA
/guard:cf
```

Unicode only:

```text
UNICODE
_UNICODE
```

Использовать Win32 `W` APIs.

---

# 30. Windows libraries

Разрешены системные библиотеки:

```text
kernel32
user32
gdi32
comctl32
shell32
advapi32
ws2_32
iphlpapi
icmp
dnsapi
shlwapi
```

Подключать только реально используемые.

---

# 31. Windows network backend

Реализовать Windows network backend через системные API.

Для interfaces:

```text
GetAdaptersAddresses
```

Для neighbor table:

```text
GetIpNetTable2
```

Для ARP / neighbor resolution:

```text
ResolveIpNetEntry2
```

или при необходимости:

```text
SendARP
```

Для ICMP:

```text
IcmpCreateFile
IcmpSendEcho2
```

Для DNS:

```text
GetNameInfoW
```

Для TCP:

```text
Winsock2
```

Не запускать:

```text
ping.exe
arp.exe
ipconfig.exe
nslookup.exe
nmap.exe
```

и затем парсить stdout.

Использовать API напрямую.

---

# 32. Concurrency

UI никогда не должен блокироваться network scan.

Windows backend может использовать Windows Thread Pool.

Не создавать отдельный thread на каждый IP.

Должен существовать configurable concurrency limiter.

Default:

```text
64 probes simultaneously
```

Допустимый диапазон:

```text
8-256
```

Результаты worker threads передавать UI через очередь событий.

UI обновляет controls только из UI thread.

---

# 33. Внутренняя архитектура

Структура проекта:

```text
/src

    /core
        scanner.c
        scanner.h

        host.c
        host.h

        network.c
        network.h

        profile.c
        profile.h

        ports.c
        ports.h

        vendor.c
        vendor.h

        monitor.c
        monitor.h

        event_queue.c
        event_queue.h

    /platform

        /windows
            win_main.c
            win_ui.c
            win_ui.h

            win_net.c
            win_net.h

            win_dns.c
            win_dns.h

            win_ping.c
            win_ping.h

            win_arp.c
            win_arp.h

            win_process.c
            win_process.h

    /resources
        app.rc
        resource.h

/tests
/tools
/docs
```

---

# 34. Platform abstraction

Core не должен включать:

```c
#include <windows.h>
```

Ни в одном файле `/core`.

Создать platform interface.

Пример:

```c
typedef struct platform_net_ops {
    int (*enumerate_interfaces)(...);
    int (*resolve_neighbor)(...);
    int (*icmp_probe)(...);
    int (*tcp_probe)(...);
    int (*reverse_dns)(...);
    uint64_t (*monotonic_ms)(void);
} platform_net_ops_t;
```

Core получает указатель на implementation.

Windows реализует его в:

```text
platform/windows/
```

Это обязательное архитектурное требование.

---

# 35. Будущие платформы

Архитектура должна позволять позднее добавить:

```text
platform/linux/
platform/macos/
```

без изменения scanner model и основной логики.

План:

## Linux

Backend:

- sockets;
- netlink;
- `/proc` только где оправдано;
- ICMP;
- ARP/neighbor;
- epoll.

GUI решается отдельно в будущем.

## macOS

Backend:

- BSD sockets;
- routing/neighbor API;
- ICMP;
- kqueue.

GUI — отдельный native frontend.

Не выбирать сейчас общий тяжёлый cross-platform GUI framework только ради будущей поддержки платформ.

Переносится scanner core.

UI для каждой ОС может быть native.

---

# 36. IPv6

Архитектура host address должна сразу поддерживать:

```text
IPv4
IPv6
```

Но v1.00:

- активный scanner ориентирован прежде всего на IPv4 LAN;
- IPv6 neighbor entries можно отображать;
- массовое active IPv6 scanning не реализовывать.

Не пытаться перебрать IPv6 subnet.

Полноценный IPv6 discovery — отдельная будущая фаза.

---

# 37. Data model

Пример:

```c
typedef struct host_info {
    ip_address_t address;

    char hostname[256];

    uint8_t mac[6];
    bool has_mac;

    char vendor[128];

    os_family_t os_family;
    uint8_t os_confidence;

    bool online;
    bool new_device;

    uint32_t latency_ms;

    uint64_t first_seen;
    uint64_t last_seen;

    service_flags_t services;
} host_info_t;
```

Все структуры должны иметь чёткое ownership.

Не использовать глобальное mutable состояние без необходимости.

---

# 38. Memory safety

Несмотря на C:

- проверять длины всех буферов;
- не использовать `strcpy`;
- не использовать `sprintf`;
- не использовать небезопасные scanf patterns;
- проверять integer overflow;
- проверять allocation results;
- использовать bounded functions;
- сетевые данные считать недоверенными.

Все parsers должны работать с:

```text
pointer + explicit length
```

а не предполагать корректную null termination.

---

# 39. Error handling

Network errors — нормальная часть работы.

Не показывать MessageBox на каждый timeout.

Использовать внутренние error codes.

Например:

```text
SCAN_OK
SCAN_TIMEOUT
SCAN_UNREACHABLE
SCAN_REFUSED
SCAN_NO_ROUTE
SCAN_CANCELLED
SCAN_SYSTEM_ERROR
```

UI показывает только значимые ошибки.

---

# 40. Cancel

Кнопка:

```text
STOP
```

обязана быстро останавливать scan.

Не ждать завершения всех timeout последовательно.

Scanner должен поддерживать cancellation token/state.

После Cancel:

- workers завершаются;
- UI остаётся responsive;
- частично полученные результаты сохраняются.

---

# 41. Performance

Ориентир для обычной `/24` LAN:

первые найденные устройства должны появляться практически сразу.

Полный scan не должен ждать завершения всех DNS lookups.

Pipeline:

```text
DISCOVERY
   ↓
HOST FOUND
   ↓
show immediately
   ↓
hostname lookup
   ↓
vendor lookup
   ↓
service probes
   ↓
update row
```

Не делать:

```text
scan everything
wait
resolve everything
wait
show table
```

---

# 42. Network friendliness

Не создавать агрессивную нагрузку.

Default scan должен быть пригоден для обычной офисной сети.

Ограничивать:

- parallel requests;
- probe rate;
- retries.

Default retries:

```text
1
```

Monitoring должен быть значительно мягче initial scan.

---

# 43. Privileges

Обычная работа должна происходить без Administrator.

UAC не запрашивать.

Если какая-либо дополнительная функция в будущем потребует elevation — она должна быть отдельной и явной.

Первый релиз не должен требовать elevation вообще.

---

# 44. Не делать в v1.00

Чтобы проект не разросся, запрещено добавлять до стабильного MVP:

- packet capture;
- vulnerability scanner;
- password auditing;
- credential storage;
- WMI remote administration;
- SSH client;
- встроенный RDP client;
- SNMP manager;
- Wake-on-LAN;
- remote shutdown;
- file transfer;
- remote shell;
- web dashboard;
- database;
- SQLite;
- service;
- cloud;
- account;
- updater;
- plugins.

Сначала должен получиться законченный LAN scanner.

---

# 45. MVP v0.1

Первый testable milestone:

Программа:

1. запускается одним EXE;
2. показывает network interfaces;
3. определяет local IPv4 subnet;
4. сканирует `/24`;
5. находит active hosts;
6. показывает IP;
7. показывает MAC;
8. делает reverse DNS;
9. показывает ping;
10. UI не зависает;
11. scan можно Stop;
12. double click показывает host details.

Никаких profiles/monitoring/reporting до выполнения этого milestone.

---

# 46. Milestone v0.2

Добавить:

- MAC vendor;
- TCP fallback discovery;
- common service detection;
- sorting/filtering;
- single host scan;
- context menu;
- Open RDP/Web/SMB.

---

# 47. Milestone v0.3

Добавить:

- monitoring;
- NEW/OFFLINE state;
- network log;
- first seen / last seen;
- rescan host.

---

# 48. Milestone v0.4

Добавить:

- profiles;
- CSV export;
- TXT export;
- HTML export;
- statistics;
- compact GDI graphs.

---

# 49. Milestone v1.00

Стабилизировать:

- Windows 10;
- Windows 11;
- DPI scaling;
- multiple NICs;
- Wi-Fi/Ethernet;
- cancellation;
- shutdown during scan;
- long hostnames;
- unavailable DNS;
- ICMP-blocked hosts;
- offline networks;
- /23-/24 ranges;
- repeated scans;
- memory/resource leaks.

После этого выпускать первый stable release.

---

# 50. Tests

Core должен тестироваться отдельно от Win32 UI.

Создать fake network backend.

Например:

```text
192.168.1.1  online  MAC A  HTTP
192.168.1.10 online  MAC B  RDP SMB
192.168.1.20 offline
```

Проверять:

- discovery merge;
- duplicate hosts;
- state transitions;
- NEW detection;
- ONLINE→OFFLINE;
- OFFLINE→ONLINE;
- cancellation;
- range parser;
- port parser;
- profile parser.

Запуск через CTest.

---

# 51. Resource leak tests

Особенно проверять:

- HANDLE;
- sockets;
- events;
- ICMP handles;
- threadpool objects;
- GDI objects;
- menus;
- icons.

После нескольких сотен scans число handles не должно постоянно расти.

---

# 52. Logging разработки

Debug build может писать:

```text
LANScope-debug.log
```

Release по умолчанию ничего на диск не пишет.

Debug log должен содержать:

- stage;
- IP;
- elapsed time;
- result/error code.

Никогда не логировать credentials, потому что их вообще не должно быть.

---

# 53. UI responsiveness

Недопустимо:

- network request из `WndProc`;
- `Sleep()` в UI thread;
- DNS lookup из UI thread;
- полный список перерисовывать после каждого probe.

Использовать batch/update notifications.

---

# 54. Качество кода

Codex должен:

- избегать premature abstractions;
- избегать огромных framework-like layers;
- не вводить dependency injection framework;
- не создавать собственный object system на C;
- не использовать macro magic без необходимости.

Предпочитать простой понятный C.

Функции должны быть небольшими.

Platform boundaries — явными.

---

# 55. Documentation

Создать:

```text
README.md
ARCHITECTURE.md
BUILD.md
PROJECT_STATE.md
```

README:

- что делает программа;
- screenshot;
- download/build;
- ограничения.

ARCHITECTURE:

- core;
- Windows backend;
- UI event flow;
- scanner pipeline.

BUILD:

точные команды сборки.

PROJECT_STATE:

- что реализовано;
- что тестировалось;
- известные проблемы;
- следующий milestone.

---

# 56. Главный UX критерий

Пользователь не обязан понимать networking internals.

Он должен иметь возможность:

```text
1. Запустить LANScope.exe.
2. Нажать Scan.
3. Получить список устройств.
```

Всё.

Advanced options не должны мешать этому сценарию.

---

# 57. Definition of Done v1.00

Релиз считается готовым только если:

- один portable EXE;
- Windows 10 x64 работает;
- Windows 11 x64 работает;
- нет external runtime;
- нет installer;
- нет admin requirement;
- нет Npcap;
- нет OpenGL;
- нет .NET;
- нет Chromium/WebView;
- обычная локальная сеть обнаруживается автоматически;
- устройства появляются во время scan;
- ARP + ICMP + TCP fallback работают;
- IP/MAC/hostname/vendor отображаются;
- common services определяются;
- single host scan работает;
- monitoring работает;
- NEW/OFFLINE changes отслеживаются;
- profiles работают;
- CSV/TXT/HTML export работает;
- Stop немедленно останавливает scan;
- UI не зависает;
- нет подтверждённых resource leaks;
- Release EXE соответствует цели по минимальному размеру;
- scanner core не зависит от Win32 и пригоден для будущих Linux/macOS backend.

---

# 58. Инструкция Codex

Начать с анализа ТЗ и структуры проекта.

Не пытаться реализовать всё сразу.

Последовательно выполнить:

```text
v0.1
↓
build
↓
tests
↓
manual testable EXE
↓
v0.2
↓
tests
↓
v0.3
↓
tests
↓
v0.4
↓
stabilization
↓
v1.00
```

После каждого milestone:

1. собрать Release;
2. запустить automated tests;
3. перечислить реализованные функции;
4. перечислить известные проблемы;
5. обновить PROJECT_STATE.md;
6. дать путь к готовому EXE для ручной проверки.

Не останавливать разработку ради косметических вопросов, если решение можно разумно принять самостоятельно.

При споре между:

```text
больше функций
```

и

```text
меньше, быстрее, проще
```

для версии 1.00 выбирать:

```text
меньше, быстрее, проще.
```