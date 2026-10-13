# Current release work — 13 October 2026

Linux x64 **1.3.0 stable** is built locally on Windows, maximally UPX-packed and tested on the provided Ubuntu 26.04.1 VM. Exact final ZIP SHA-256: `743a896be8fe06118c0ec1ff4cf48ea492f14d19b2395003117f8b47523c5fc2`. Native suites and 39 GTK workflows passed against the final binary. 120 scheduled monitoring rounds (~10 minutes) kept FD 6 → 6, RSS 11548 → max 11680 KiB; Stop max 11 ms. Final real-LAN screenshots: docs/images/linux-1.3.0.png and windows-1.2.0.png (published Windows packed binary, not mock data).

The Unix audit additionally removed all strcpy/sprintf calls from src, added a bounded non-truncating text helper, fixed PTR CNAME/case handling, source/MAC snapshot locking, sparse port profile round-trips above 8 KiB with a 64 KiB serialization limit and full Unicode Windows field reads. Workers are capped by actual target count. Both ports now have a native log table exposing the complete 10000-event ring. Windows regression: all 15 test cases passed; profile/UI/file tests rerun after fixing the newly exposed parser limit. Historical failed test outputs are retained privately.

Public Windows remains stable 1.2.0 (three architecture ZIPs). Linux 1.3.0 is a separate stable release containing one tested Linux ZIP. macOS universal **1.3.0-rc.3** remains local; code/resource seals and DMG extraction/modes passed. Its native test still needs the user. Do not publish macOS based on cross-compilation or the old port.1 screenshot. macOS final screenshot is also pending from the user.

Linux release proof: dist/linux-release-validation.json; native logs and screenshots in build/validation and docs/images. Publisher gates exact asset SHA/native-pass/feature-complete. Non-ASCII OEM NBNS names remain unknown; mDNS/active IPv6 are future scope. Ubuntu 22.04 runtime and physical HiDPI/NIC-unplug are not claimed as tested. Native monitoring is a 10-minute bounded check, not hours/days of GUI stress.


---

The sections below are historical records; use the current state above for release decisions.

# Состояние проекта

**Look@LAN-ng stable 1.2.0, Windows x86/x64/ARM64, 12 октября 2026.** Пользователь явно разрешил выпуск stable с перечислением непроверенных пунктов. Отдельная Windows 10/11 matrix не объявляется пройденной. Это решение сохранено в AGENTS.md и docs/RELEASE_CHECK.md.

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

Публичный repository: https://github.com/nr084/Look-LAN-ng . Первый GitHub release: https://github.com/nr084/Look-LAN-ng/releases/tag/v1.2.0 . Название приложения Look@LAN-ng сохранено; @ недопустим в repository name.

| Архитектура | ZIP | ZIP bytes | Packed EXE bytes | Raw bytes |
| --- | --- | --- | --- | --- |
| x86 | look@lan-ng-x86-1.2.0.zip | 414358 | 423936 | 1076224 |
| x64 | look@lan-ng-x64-1.2.0.zip | 436044 | 445440 | 1125376 |
| ARM64 | look@lan-ng-arm64-1.2.0.zip | 511568 | 546816 | 1128448 |

Все ZIP скачаны обратно из опубликованного релиза: SHA-256 совпадают с GitHub digest и release notes; каждый содержит только Look@LAN-ng.exe нужной архитектуры и версии 1.2.0. Packed binaries: dist/1.2.0/<architecture>/Look@LAN-ng.exe; raw diagnostic: build/unpacked/<architecture>/Look@LAN-ng.exe. Полные hashes/packing/test results: dist/release-info.json и dist/look@lan-ng-1.2.0.json. Прежние local ZIP сохранены как исторические artifacts; flat dist/Look@LAN-ng.exe обновлён до GitHub x64 binary, пользовательские процессы не завершались.

UPX latest stable 5.2.1, official archive SHA-256 verified; --ultra-brute --lzma. Integrity и четыре packed smoke modes прошли для каждой архитектуры. ASLR/NX на всех targets, HIGHENTROPYVA только x64/ARM64; Guard CF отсутствует, static CRT/only system DLL подтверждены по raw imports. Несжатые binaries также сохранены в CI artifacts.

GitHub workflow: https://github.com/nr084/Look-LAN-ng/actions/runs/38256307207 — success. CTest: x86 15/15 (56.39 s), x64 15/15 (51.09 s), ARM64 native 15/15 (45.05 s). Всего 45/45 и 12/12 packed smoke modes. x86/x64 runner Windows Server 2022, ARM64 runner Windows 11. ARM64 Debug и Release NBNS regression также passed: прямое чтение wire GROUP bit устраняет воспроизведённый optimized MSVC 2026 word/mask issue, high/low byte cases покрыты тестами.

После уточнения пользователя все новые builds локальные; прежний GitHub compilation workflow удалён. Публикация готовых assets — tools/publish-local-release.ps1. Windows distribution остаётся три архитектурных ZIP; README/release notes RU/EN.
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

Публикация GitHub: публичный nr084/Look-LAN-ng (GitHub repository names не поддерживают @; имя приложения Look@LAN-ng сохранено). README и release notes на русском/английском. Уточнение artifact names: look@lan-ng-x86-<version>.zip, look@lan-ng-x64-<version>.zip, look@lan-ng-arm64-<version>.zip, один EXE внутри каждого. MSVC x86 build и 15/15 tests passed locally, 54.73 s; x64/ARM64 CI результаты подтверждены выше. HIGHENTROPYVA только для 64-bit, x86 ASLR/NX; исправлен compiler warning потенциально uninitialized IP.

## macOS / Ubuntu local port preview 1.2.0-port.1

Compiled locally on Windows: Cocoa x86_64+arm64 universal, GTK3 x86_64-linux-gnu with Ubuntu 22.04 baseline. Common POSIX background ICMP/TCP backend / MAC cache / profiles / reports / RU-EN added. Full feature parity is pending; docs/PORT_TESTING.txt lists it explicitly. Native Mac mount/launch/LAN testing assigned to user, Linux native testing awaits user machine. Code-page/Info.plist/resource seals, universal architectures, genuine HFS+ DMG LZMA round-trip and execute modes verified. macOS DMG 721125 bytes, SHA256 1c313aa374fc6d326d5128f8fc6871cd0c8106064c070405ae36ed4a9baccee4. Linux UPX 5.2.1 --ultra-brute --lzma / integrity passed, ZIP 323769 bytes, SHA256 65338c87f2ad4c82fc8eb2e27c470f74ae9674cb572c158b4eb2c4e87cce7b58. Mac UPX unsupported in normal mode for both architectures; no force used. Ad-hoc, not notarized. Windows x64 rebuild / 15 tests passed, 51.66 s, after lan_host_t rename for Mach collision.

GitHub compilation workflow removed per local-build instruction. Local build/package/publish scripts added. Old local release snapshots and temporary paths cleaned; current stable Windows artifacts and all current raw diagnostic / port builds retained. Published historical GitHub releases untouched.

Preview опубликован: https://github.com/nr084/Look-LAN-ng/releases/tag/v1.2.0-port.1 . Оба assets скачаны обратно, SHA-256 совпадают с GitHub; release prerelease=true/draft=false, stable latest остаётся v1.2.0. Cocoa ARC property ownership дополнительно проверена compiler AST (strong), delegate lifetime explicit. Нативные macOS/Linux тесты не выполнялись.

## Native port validation and development port.2 — 12 October 2026

Пользователь предоставил Ubuntu VM; OS 26.04.1 x86_64. Native выполнение обычным пользователем: 5 portable C suites + POSIX runtime suite passed. Own-IP scan/rescan, invalid-range recovery, 100 Scan/Stop cycles, FD 6 → 6, max Stop 11 ms, idempotent bounded close. Исходный published Linux port.1 ZIP hash проверен; raw и UPX packed smoke passed. Gateway scan подтвердил ICMP latency/MAC/vendor/TCP results. 23 GTK GUI checks passed: profiles/export/filter/range/F5/Stop/close/RU-EN/system preference persistence. Подробные ограничения и воспроизводимый driver: docs/LINUX_VALIDATION.md, tools/test-linux-gui.py. Xvfb/Openbox session и config изолированы от пользовательских настроек. Credentials не сохранены.

Воспроизведена и исправлена пустая ошибка F5 during active scan. Uppercase .HTML/.TXT поддержаны; toolbar сохраняет подписи и RU/EN tooltips. OS guess больше не считает TTL=0 признаком Linux; portable regression unknown/64 TTL added. Windows x64 CTest 15/15, 52.74 s. Компиляция обеих Mac architectures и Linux выполнена локально Windows.

Пользователь подтвердил screenshot запуска/реального LAN discovery macOS port.1. Версия macOS/CPU не указаны, оба slice native и полный matrix не подтверждены. Universal port.2 DMG подготовлен с TTL correction и новой CFBundleVersion .2; LZMA/HFS+, code seals/round-trip passed; требует отдельного Mac теста.

По последнему указанию Linux preview больше не публикуется. Новый Linux ZIP остаётся development artifact, full parity ещё отсутствует: remote DNS/NetBIOS, Ping/Trace, automatic monitoring, profile editor, context actions, device notes/history UI, statistics; также synchronous export/profile I/O. Publisher запрещает Linux prerelease и требует matching hash/native-pass/feature-complete report для stable. Исторический GitHub port.1 не изменён.

Последующее уточнение пользователя: временный GitHub port release удалить. Release v1.2.0-port.1 (оба assets) и соответствующий remote tag удалены, API подтвердил: только stable v1.2.0 с тремя Windows ZIP. port.2 на GitHub не публиковалась. Final Windows/Linux screenshots снять с окончательных release builds перед выкладкой, macOS screenshot предоставит пользователь. build/validation/linux-runtime.png — локальный материал текущей native проверки, не final release marketing image. Собственная папка тестов VM и старые локальные port.1 поставки удалены; текущие ZIP/DMG/raw diagnostics сохранены.
