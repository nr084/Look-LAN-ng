# Состояние проекта

**Look@LAN-ng stable 1.2.0, Windows x64, 12 октября 2026.** Пользователь явно разрешил выпуск stable с перечислением непроверенных пунктов. Отдельная Windows 10/11 matrix не объявляется пройденной. Это решение сохранено в AGENTS.md и docs/RELEASE_CHECK.md.

## Готово

- C17 core без Windows headers, native Win32 RU/EN, static CRT, PerMonitorV2/asInvoker; нет установки, admin requirement, облака, runtime/driver dependencies.
- NIC detection, selected-source IPv4 ARP/ICMP/TCP discovery, incremental rows, отдельный reverse DNS, встроенный vendor lookup (40316 MA-L prefixes), cautious OS/service hints.
- Single Host, quick/custom ports, sorting/filter, Web/HTTPS/RDP/SMB/copy actions.
- Monitoring с ONLINE/OFFLINE/NEW/CHANGED/UNKNOWN, two-miss threshold, First/Last Seen, bounded History/Log (10000). Cancel/error/DNS/cache не создают ложного online/offline.
- Versioned UTF-8 profiles, до 64 ranges с union/dedup, timeouts, concurrency 8–256, interval, notes, stable adapter ID, native RU/EN editor и portable profile folder. Недоступный NIC требует явного выбора.
- CSV/TXT/HTML со snapshot/background writer, UTC, escaping/formula guard и atomic replacement. Cancel/error сохраняет назначение; GDI statistics.
- ARP pool: 16 persistent workers/256 requests. Caller выходит по Stop, queued work удаляется, running work сохраняется до завершения native API; copied source/interface предотвращает доступ к следующей scan session.
- ICMP: async event, bounded 256 active+retired requests, сохранение payload/reply/handles до completion. DNS: async forward/PTR, 3 s waiting limit, cancel по Stop/generation/shutdown. UI timer освобождает все backend workers без UI wait.
- Общий probe pacing: ~250/s initial, ~62.5/s monitoring; single generation, bounded concurrency. OS ARP retries не контролируются приложением.
- Исправлены toolbar text widths, узкий layout/filter overlap, premultiplied alpha status dots. Original SVG/ICO 16–256 px: современный radar/network icon с лёгкой ретро-отсылкой. Генератор tools/generate-icons.ps1.
- © nr084, 2026 и версия в About/EXE; год и версия генерируются CMake. GUARD_CF отсутствует по указанию пользователя; ASLR/NX/HIGHENTROPYVA сохранены.

## Добавлено в 1.1

- Главное окно: native toolbar custom draw с полными readable disabled captions, font-based button height/width, focus/tooltips, более компактная верхняя панель. TreeView остаётся enabled; смена NIC во время scan/monitoring запрещена через TVN_SELCHANGING. Исправление проверено native off-screen preview в состоянии scan.
- NetBIOS Node Status fallback после reverse DNS: unicast UDP 137, source bind, transaction/flags/RDLENGTH/name-table validation, non-group workstation name, 300 ms и cancel/generation polling. Parser в portable core; OEM → UTF-8 в Windows backend.
- Отдельный Ping: четыре samples, примерно секундные интервалы, background worker. История automatic/manual ping — ring 10000 по IP/interface, до 60 последних в панели.
- Traceroute/Trace: native IcmpSendEcho2Ex с TTL 1–30, reply IP/RTT/status, destination stop, cancel. IP_OPTION_INFORMATION живёт в heap request до completion, TTL exceeded не объявляется Echo success/ONLINE. Последний маршрут текущей сессии показывается в Trace.
- Device notes в General/context action: до 511 UTF-8 bytes, dictionary 1024 IP/interface keys. Notes и ping history — session-only, profile notes отдельно сохраняются. Late diagnostic results от старой session игнорируются.
- Open SSH: optional System32/OpenSSH/ssh.exe, validated username/numeric IP, без сохранения credentials; клиент не нужен для работы сканера.
- Close/Stop отменяют tools job; backend pool освобождается только после завершения диагностики. Следующий monitoring round ждёт tools completion.

## Проверки

MSVC v143 x64 Release собран без warnings. Полный CTest: **15/15 passed, 53.82 s**; финальные UI tests с выбором языка и glyph paint на DPI: **4/4 passed**. Core, features, monitor, profile/report, neighbor, ping, resolver, backend, UI startup/close-scan/close-tools/DPI и files.

Details/NBNS tests: scoped notes/limits/clear, ring eviction, wildcard encoding, truncated/wrong-transaction/group/control/malformed packets. Tools tests: two-hop TTL progression, destination stop, 30 silent-hop cap, four ping samples и pending cancellation. Реальная integration: четыре own-IP Echo replies и one-hop native traceroute. UI smoke проверяет notes/Ping/Trace panels и close-tools. SSH не запускался интерактивно, NetBIOS server/live multi-hop routing matrix не проверялась.

Core включает /23-/24-/31-/32/0, bounded parsing/union/overflow, cancel/cache/dedup/capacity, ports/refused/open, vendor validation, monitor state/scopes/ring, UTF-8 profile round-trip/error rejection, CSV/HTML/TXT escaping/writer failure, statistics. Files: Unicode paths, immutable snapshot, cancellation preserves destination; fixtures очищены.

ARP double блокирует ResolveIpNetEntry2 после cancellation: caller выходит, output сохраняется, delayed completion/shutdown безопасны; 100 probes без handle growth. ICMP doubles: pending timeout 5000 ms, prompt cancellation (<200 ms в test), отсутствие premature close, предел 256 и 320 retire/reap cycles. Resolver doubles: sync/async/errors, pending cancellation/timeout, insufficient buffer, preserved outputs и cleanup. Эти doubles не заменяют реальную offline/DNS LAN matrix.

Backend делает только self IPv4/localhost/private local listener. После **500 warm-up** — **500 measured Scan/Stop**, выборки каждые 100 после quiescence, без дальнейшего handle growth. В отдельном диагностическом прогоне: **180 → 178**, без увеличения после quiescent sampling. В первом коротком прогоне наблюдался bounded +2 рост; более длинный прогрев и дальнейшее снижение согласуются с Windows-managed lazy resources, точное происхождение не трассировалось. 30 init/shutdown cycles (15 с coordinator) проходят без handle growth. Это ограниченный stress, не hours/days LAN test. Полная интеграция выполнялась вне sandbox, который ранее блокировал local TCP.

UI: EN/RU, 253-byte hostname, filter/tabs/states/ack/history/log, profile dialog, GDI statistics. DPI smoke — **100 synthetic WM_DPICHANGED transitions** на 96/120/144/168/192 DPI, два размера окна, bounds/overlap, bounded GDI/USER resources. Hidden own-IP close-scan освобождает backend до exit. Native preview inspected: docs/images/ui-preview.png, явно помеченные sample data, без scan/INI. Это не physical-monitor matrix.

## Поставка

- dist/Look@LAN-ng-1.2.0-win64.zip: **436001 bytes**.
- dist/1.2.0/Look@LAN-ng.exe: **445440 bytes** (435 KiB).
- build/unpacked/Look@LAN-ng.exe: **1125376 bytes**, diagnostic raw.
- dist/release-info.json и dist/Look@LAN-ng-1.2.0-win64.json: параметры/размеры/SHA-256.

Latest stable UPX **5.2.1**, official API/digest checked. **--ultra-brute --lzma**, 39.58% raw size. Integrity/startup/DPI/close-scan/close-tools packed tests passed. Numeric FileVersion и ProductVersion: 1.2.0; LegalCopyright: © nr084, 2026. ZIP содержит единственный EXE с совпадающим SHA-256. PE x64, ASLR/NX/HIGHENTROPYVA, без Guard CF, only system DLL. UPX stages и временные журналы/bitmap очищаются после финальной проверки; полезные artifacts и прежние ZIP сохранены. Никакой загрузки во внешние сервисы не выполнялось. Flat dist/Look@LAN-ng.exe успешно обновлён; versioned EXE находится в dist/1.2.0. Metadata latest_exe_updated=true; пользовательские процессы не завершались.

## Явно непроверенные пункты и ограничения

- Текущее окружение: Windows build **26300**, DisplayVersion **26H2**. Отдельные Windows 10/11, Wi-Fi/Ethernet/NIC unplug, physical DPI monitors и длительные LAN/leak sessions **не проверены**. Пользователь разрешил stable с этой оговоркой.
- Stop освобождает scan workers, но не прерывает native ARP/ICMP IO. Close может ждать их остаточный timeout и OS completion, оставаясь responsive. Жёсткий deadline любых Windows providers не гарантируется.
- Optional IPv6 neighbor display, future mDNS и disk history не реализованы. Активное сканирование IPv4; service names по TCP port number, OS probabilistic, MA-M/MA-S vendor registries не включены.
- Monitoring не повторяет service scan, TCP fallback только known hosts; новые TCP-only hosts могут потребовать manual Scan. Interval — пауза после completion.
- Table 65536, DNS queue 4096, log 10000; хранятся первые 64 open ports с truncation marker. Port probes последовательны внутри host worker. Profile до 64 KiB, name 255 UTF-8 bytes, notes 2047; export path MAX_PATH-48.
- Готов только win64. Win32/Linux ZIP и macOS DMG остаются будущими платформами.

Таблица соответствия функциям ТЗ: docs/SPEC_STATUS.md. Дальнейшая validation matrix и критерии: docs/RELEASE_CHECK.md. Спецификация: docs/SPECIFICATION.md; архитектура: ARCHITECTURE.md; сборка: BUILD.md.



В 1.2 добавлены оригинальные геометрические toolbar icons (radar/stop/refresh/host/monitor/export), сглаженные supersampling 4x и масштабируемые вместе с DPI. Подписи, отключённые состояния и keyboard focus сохранены. В узком окне фильтр переносится на вторую строку.

Язык: **Вид → Язык / View → Language** → **Язык системы / System language (default)**, **English**, **Русский**. По умолчанию язык определяется по Windows UI language: русский → русский, любой другой → английский. Выбор применяется сразу и записывается в INI как `system`, `ru` или `en`; system mode остаётся автоматическим при следующих запусках. Ранее сохранённый явный выбор ru/en сохраняется. Profile editor, меню, таблица, вкладки, статистика, ошибки и диалоги используют выбранный язык; пользовательские имена/данные не переводятся.

Публикация GitHub: публичный nr084/Look-LAN-ng (GitHub repository names не поддерживают @; имя приложения Look@LAN-ng сохранено). README и release notes на русском/английском. Уточнение artifact names: look@lan-ng-x86-<version>.zip, look@lan-ng-x64-<version>.zip, look@lan-ng-arm64-<version>.zip, один EXE внутри каждого. MSVC x86 build и 15/15 tests passed locally, 54.73 s; x64/ARM64 CI результаты будут записаны после завершения. HIGHENTROPYVA только для 64-bit, x86 ASLR/NX; исправлен compiler warning потенциально uninitialized IP.
