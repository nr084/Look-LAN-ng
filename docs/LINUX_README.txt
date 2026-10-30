Look@LAN-ng 1.4.0 — Linux x64
© nr084, build year / год сборки

RU: Распакуйте ZIP и запустите ./LookLAN.sh в графической сессии с GTK 3.
Права root и установка приложения не нужны. Если распаковщик потерял execute
mode: chmod +x LookLAN.sh 'Look@LAN-ng'. Язык: System / English / Русский.

Разовый запуск/сканирование не создаёт INI и папки настроек. «Сохранить профиль»
сохраняет профиль v2 в profiles рядом с программой и включает постоянный режим.
Папка приложения должна быть доступна для записи. Единственный профиль
выбирается автоматически при запуске. История хранится рядом с профилем в
файле .lanprofile.lanhistory: хосты, изменения и последние 10000 событий.
Хосты из истории не считаются проверенными ONLINE до нового сканирования.
Для резервной копии сохраняйте весь profiles. Старые профили v1 не читаются.

EN: Extract the ZIP and run ./LookLAN.sh in a graphical session with GTK 3.
No root privileges or application installation required. If executable modes
were lost, run: chmod +x LookLAN.sh 'Look@LAN-ng'.

Temporary launch/scanning creates no INI or settings folders. Saving a v2
profile into profiles beside the app enables persistence; this folder must
be writable. A sole profile is selected automatically on startup. Associated
.lanprofile.lanhistory files retain hosts, changes and the latest 10000 events.
Restored hosts show HISTORY until checked again. Back up the complete profiles
folder. Legacy v1 profiles are not supported. Device notes and Ping/Trace
samples remain session-only; profile notes persist in the settings file.

ARP/ICMP/TCP discovery, DNS/NetBIOS, IEEE MAC/OUI, ports, cautious OS hints,
sorting/filtering, monitoring, Ping/Trace, history/log/statistics and export.
No cloud, accounts, telemetry or background services. mDNS/active IPv6 are
future work. Ubuntu 26.04.1 VM tested; Ubuntu 22.04+ is a build baseline,
not a claim that every distro/hardware/DPI/NIC combination was tested.
Source, release hashes and validation: https://github.com/nr084/Look-LAN-ng
