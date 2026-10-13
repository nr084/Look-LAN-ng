Look@LAN-ng 1.3.1-rc.1 — локальный кандидат / local candidate

Русский
Обязательная функциональность ТЗ реализована для Windows x86/x64/ARM64,
Linux x64 и macOS universal (Intel + Apple Silicon). Кандидат собран локально
на Windows. В этом этапе по прямому указанию пользователя проверки не
выполнялись: CTest, GUI, runtime, smoke, UPX -t, подписи и извлечение DMG
не запускались. Готовность реализации не означает подтверждённую работу.
Эти пакеты не опубликованы как готовый stable release.

Windows: ZIP содержит один Look@LAN-ng.exe, установка не нужна.
Linux: распакуйте ZIP и запустите ./LookLAN.sh; нужны системные GTK 3.
macOS 13+: перенесите Look@LAN-ng.app из DMG в папку приложений.
Mac-приложение подписано ad-hoc, без notarization. UPX не применяется к
неподдерживаемому Mach-O; DMG сжат LZMA. Windows/Linux максимально сжаты
stable UPX, но целостность и запуск этих packed binaries не проверены.
Несжатые диагностические сборки сохранены у разработчика.

Выберите интерфейс и нажмите «Сканировать». Имена определяются отдельно,
устройства появляются постепенно. Старый ARP-кэш сам по себе не означает
«В СЕТИ». RU/EN доступны в выборе языка; по умолчанию используется язык
системы: русский для русской системы, английский для всех остальных.
Профили сохраняют диапазоны, интерфейс, порты, таймауты и интервал.
Экспорт CSV/TXT/HTML выполняется в фоне. История и заметки устройств —
текущей сессии. mDNS, активный IPv6 и история на диске не входят в v1.00.
© nr084; год сборки указан в «О программе» и свойствах приложения.

English
Required specification features are implemented for Windows x86/x64/ARM64,
Linux x64 and macOS universal (Intel + Apple Silicon). Built locally on Windows.
At the user's request, no checks were run in this stage: no CTest, GUI/runtime,
smoke tests, UPX -t, signature verification or DMG extraction checks.
Implementation completion is not verified runtime readiness. These packages
are local candidates, not published stable releases.

Windows: extract and run the single EXE. Linux: extract and run ./LookLAN.sh
with system GTK 3 libraries. macOS 13+: copy the universal application out
of the DMG. Mac signing is ad-hoc, without notarization. Mach-O is preserved
when stable UPX refuses it; DMG uses LZMA. Windows/Linux binaries use maximum
stable UPX compression, with packed integrity/startup unverified. Raw builds
are preserved for diagnostics.

Choose an interface and Scan. Background naming, incremental hosts,
monitoring, scoped session details, profiles and CSV/TXT/HTML export are
included. System/Russian/English language choice is saved; Russian system
locale defaults to Russian, all other locales to English. Cached neighbors
alone are unconfirmed. mDNS, active IPv6 and disk history remain future scope.
© nr084; the generated build year is shown in About and application metadata.
