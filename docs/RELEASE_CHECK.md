# Stable 1.2.0 — доказательства и непроверенные пункты

12 октября 2026 пользователь явно выбрал: **«Выпустить stable с явно указанными непроверенными пунктами»**. Поэтому выпуск stable разрешён без отдельной Windows 10/11 matrix. Это исключение не означает, что все критерии исходного DoD проверены.

| Проверка | Результат |
| --- | --- |
| C17/MSVC x64 Release, static CRT | Собрано без warnings |
| Portable EXE, only system DLL, asInvoker | Проверены PE/imports/manifest |
| Core/ranges/ports/vendor/monitor/profiles/reports/files | Automated tests passed |
| NetBIOS fallback | Portable packet tests passed; unicast/source-bound Windows backend; live NetBIOS matrix не проверена |
| Ping/history/Trace/device notes | Tests passed; реальные own-IP Echo и one-hop traceroute; intermediate TTL и 30-hop cap через doubles |
| Open SSH | Optional Windows client, validated parameters; интерактивная SSH-сессия тестами не запускалась |
| ARP cancel при blocked API | Deterministic double; caller выходит, job живёт до completion |
| ICMP/DNS cancellation/lifetime | Doubles + self-host integration |
| /23-/24, 253-byte hostname | Core/UI tests |
| Repeated Scan/Stop | 500 warm-up + 500 measured self-host cycles; stable sampling |
| Close during scan | Hidden own-IP close-scan + 15 backend cycles с coordinator |
| Close during diagnostics | Hidden own-IP close-tools + cancel/lifetime tests |
| DPI 100–200%, RU/EN | 100 synthetic transitions; layout/overlap/GDI/USER |
| Native appearance | Off-screen native preview inspected, sample data clearly marked |
| UPX latest stable/max compression | Integrity/startup/DPI/close-scan проверяются при упаковке |
| Windows 10 и Windows 11 отдельно | **Не проверены**; текущее окружение build 26300, DisplayVersion 26H2 |
| Physical monitors / per-monitor move | **Не проверено**; synthetic DPI не заменяет это |
| Wi-Fi/Ethernet, several NICs, unplug/reconnect | **Полная ручная matrix не проверена** |
| Unavailable DNS/ICMP-blocked/offline LAN | Doubles покрывают отдельные сценарии; **реальная matrix не проверена** |
| Длительная LAN monitoring / memory / sockets | **Не проверено в течение часов/дней**; ограниченные tests не доказывают отсутствие любых утечек |
| Win32/Linux/macOS | **Не реализованы**, будущие платформы |

Остановка освобождает scan workers, но Windows ARP/ICMP IO может завершаться позже в bounded pools. При Close UI ждёт safe cleanup, оставаясь responsive. DNS cancellation также ждёт OS completion перед освобождением storage. Жёсткий предел shutdown для любого Windows/provider не обещается.

NetBIOS fallback, traceroute/Trace, standalone Ping/history, Open SSH и device notes реализованы в 1.1. Optional IPv6 neighbor display и disk history отсутствуют. mDNS отнесён ТЗ к будущему; banner verification и точное OS fingerprinting не обещаются. Device notes/history — текущая сессия, profile notes сохраняются отдельно. Полная таблица функций: `SPEC_STATUS.md`.

Для дальнейшей проверки: на Windows 10/11 без Administrator распаковать ZIP, проверить EN/RU, F5/Esc, профили и CSV/TXT/HTML, несколько Scan/Stop/Close, monitoring ONLINE/OFFLINE и перенос окна на мониторы 100/125/150/175/200%. Записать OS build, NIC тип, сеть/число адресов, длительность и наблюдаемые ошибки. Длительные LAN tests проводить в своей сети по выбранному диапазону.


1.2: language menu commands and check state tested, system Russian/non-Russian mapping and explicit overrides tested. All six glyphs enabled/disabled rendered across 100 DPI transitions with bounded GDI/USER count. Native RU/EN previews inspected: docs/images/ui-preview.png and ui-preview-en.png, sample data and simulated scanning state; no network/INI writes. Full suite 15/15 passed (53.82 s), latest UI checks 4/4 passed (3.27 s).
