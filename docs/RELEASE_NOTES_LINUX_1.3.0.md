# Look@LAN-ng 1.3.0 — Linux x64

## Русский

Первый готовый Linux-порт: native GTK 3, IPv4 ARP/ICMP/TCP discovery с выбранного интерфейса, DNS/NetBIOS, MAC/OUI, TCP-порты и вероятностные подсказки ОС. Реализованы Single Host, мониторинг, Ping/Trace, native журнал до 10000 событий, история/заметки устройств, статистика, дерево сетей и профилей, редактор UTF-8 профилей, сортировка/фильтр и фоновый атомарный CSV/TXT/HTML export. RU/EN/System сохраняется; русский системный locale выбирает русский, остальные — английский.

Распакуйте ZIP и запустите `./LookLAN.sh` в графической сессии с системными GTK 3 libraries (Ubuntu: `libgtk-3-0`). Права root и установка приложения не требуются. Бинарник максимально сжат stable UPX 5.2.1 (`--ultra-brute --lzma`); integrity и настоящий packed startup/GUI проверены.

Проверено обычным пользователем на Ubuntu 26.04.1 x64 VM: шесть C suites, 39 GTK сценариев, fresh ARP при блокированных ICMP/TCP/UDP и stale-cache fixtures, 100 Scan/Stop, 120 запланированных monitoring rounds (~10 минут). FD 6 → 6; RSS 11548 → максимум 11680 КиБ; max Stop 11 мс. Компиляция и упаковка выполнены локально на Windows.

Ubuntu 22.04+ — baseline сборки; runtime на 22.04 отдельно не проверен. Физические HiDPI/NIC unplug не проверены. Non-ASCII OEM NetBIOS-имена остаются неизвестными; mDNS и active IPv6 — будущая фаза. Заметки/история устройств хранятся только в текущей сессии. External SSH/URI actions требуют системных обработчиков. OS hints не являются точным fingerprinting.

Windows stable 1.2.0 остаётся в [своём релизе](https://github.com/nr084/Look-LAN-ng/releases/tag/v1.2.0). Новая macOS universal rc.3 остаётся локальной до проверки пользователем; этот релиз содержит только проверенный Linux ZIP.

## English

The first completed Linux port: native GTK 3, selected-interface IPv4 ARP/ICMP/TCP discovery, async DNS/NetBIOS, MAC/OUI, TCP ports and probable OS. Includes single host, monitoring, Ping/Trace, native 10000-event log, session notes/history, statistics, network/profile tree and editor, sorting/filtering, UTF-8 profiles and atomic background CSV/TXT/HTML export. RU/EN/System preference persists; Russian locale selects Russian, other locales select English.

Extract and run `./LookLAN.sh` in a graphical session with system GTK 3 libraries (Ubuntu: `libgtk-3-0`). No app installation or root is required. Maximum stable UPX 5.2.1 compression, integrity and real packed startup/GUI tests passed. All compilation/packaging ran locally on Windows.

Native evidence on Ubuntu 26.04.1 x64 VM: six C suites, 39 GTK workflows, ARP-only/static-cache fixtures, 100 Scan/Stop and 120 scheduled monitoring rounds (~10 minutes). FD 6 → 6; RSS 11548 → maximum 11680 KiB; maximum Stop 11 ms. Ubuntu 22.04 is a build baseline, not separately runtime-tested. Physical HiDPI/NIC unplug are untested. Non-ASCII OEM NBNS names remain unknown; mDNS/active IPv6 are future work. Notes/history are session-only; SSH/URI actions need configured system applications; OS hints are probabilistic.

Windows stable 1.2.0 remains in its existing release. The new macOS universal candidate remains local pending native user validation. This release includes the tested Linux ZIP only.

## Screenshot / Скриншот

![Linux 1.3.0: actual subnet scan](https://raw.githubusercontent.com/nr084/Look-LAN-ng/main/docs/images/linux-1.3.0.png)

## SHA-256

- `look@lan-ng-linux-x64-1.3.0.zip`: `743a896be8fe06118c0ec1ff4cf48ea492f14d19b2395003117f8b47523c5fc2`

© nr084, 2026
