Look@LAN-ng 1.3.2 — Linux x64
© nr084, build year / год сборки

Русский
Распакуйте ZIP и запустите ./LookLAN.sh. Нужны графическая сессия и системные
GTK 3 libraries (Ubuntu: libgtk-3-0). Установка приложения и права root не нужны.
Если распаковщик потерял execute mode: chmod +x LookLAN.sh 'Look@LAN-ng'.
Выберите интерфейс и нажмите Сканировать/F5. Стоп/Esc отменяет операцию.

ARP/ICMP/TCP discovery, DNS/NetBIOS, MAC/OUI, TCP-порты, вероятностная ОС,
таблица/сортировка/фильтр, Single Host, мониторинг, Ping/Trace, журнал до 10000
записей, статистика, заметки, UTF-8 профили и CSV/TXT/HTML экспорт.
Язык системы: русский → русский, любой другой → английский. Явный выбор
RU/EN/System сохраняется. Настройки: ${XDG_CONFIG_HOME:-~/.config}/LookLAN-ng/.
Мониторинг мягче первоначального scan и не повторяет полный service scan.
Большие диапазоны запрашивают подтверждение. Полные порты задаются вручную
в профиле. История/заметки устройств — только текущая сессия.

Проверено на Ubuntu 26.04.1 x64 VM обычным пользователем: native C suites,
39 GTK сценариев, UPX integrity/startup, 100 Scan/Stop, 120 запланированных
циклов мониторинга (~10 минут), свежий ARP при блокированных ICMP/TCP/UDP.
Ubuntu 22.04+ — baseline сборки; runtime на 22.04 отдельно не проверялся.
Физические HiDPI-мониторы и отключение физического NIC не проверены.
ARP-кэш сам по себе не доказывает ONLINE. Неизвестные OEM NetBIOS-имена
с не-ASCII байтами не угадываются. mDNS/активный IPv6 не реализованы.
SSH открывается системным обработчиком либо внешним ssh в терминале, если
они установлены. Для Web/HTTPS/RDP/SMB нужны соответствующие системные приложения.

English
Extract and run ./LookLAN.sh in a graphical session with system GTK 3 libraries
(Ubuntu: libgtk-3-0). No application installation or root privileges are needed.
If execute modes were lost, chmod +x LookLAN.sh 'Look@LAN-ng'. Select an
interface and press Scan/F5. Stop/Esc cancels an operation.

ARP/ICMP/TCP discovery, async DNS/NetBIOS, MAC/OUI, TCP ports, probable OS,
sorting/filtering, single host, monitoring, Ping/Trace, 10000-entry native log,
statistics, session notes/history, UTF-8 profiles and CSV/TXT/HTML exports.
Russian system locale selects Russian; other locales select English. The
RU/EN/System override persists in ${XDG_CONFIG_HOME:-~/.config}/LookLAN-ng/.
Monitoring uses gentler pacing and does not repeat configured service scans.
Full port ranges are selected manually. Cached neighbors do not prove ONLINE.
Non-ASCII OEM NetBIOS names remain unknown. mDNS/active IPv6 are future work.

1.3.2 native testing: Ubuntu 26.04.1 x64 VM, ordinary user, C suites and 39 GTK
workflows, packed startup/integrity and 100 Scan/Stop cycles. Historical 1.3.0: 120 scheduled
monitoring rounds (~10 minutes), ARP-only and stale-cache fixtures; not repeated for 1.3.2. Ubuntu
22.04 is a build baseline, not a verified runtime. Physical HiDPI/NIC unplug
are untested. External SSH/URI actions require configured system applications.
Source and validation: https://github.com/nr084/Look-LAN-ng
