## Русский

Первая тестовая версия портов macOS и Ubuntu, собранная **локально на Windows**. Это preview с неполной функциональной паритетностью Windows, не стабильный выпуск портов.

- macOS 13+: native Cocoa, universal Intel x86_64 + Apple Silicon arm64, настоящий HFS+ DMG с LZMA. Ad-hoc подпись; не notarized. Code-page, Info.plist и resource hashes, обе архитектуры, права запуска и полный DMG extraction round-trip проверены. Native mount/launch/LAN проверяет пользователь на Mac.
- Ubuntu 22.04+ Desktop x64: native GTK 3 и общий C17 backend, ZIP с launcher. Latest stable UPX 5.2.1, --ultra-brute --lzma, integrity passed. Native runtime testing отложено до тестовой машины пользователя.
- Работают IPv4 NIC/range selection, source-bound background ICMP/TCP discovery, Stop/safe close, MAC cache enrichment, OUI, quick TCP services, таблица/фильтр/details, повторные scan states, UTF-8 profiles, CSV/TXT/HTML, RU/EN/system language.
- Ещё не перенесены remote DNS/NetBIOS, standalone Ping/Trace, automatic monitoring, profile editor, remote context actions, device notes/history UI, statistics и полный Win32 layout. Fresh ARP discovery Windows API не заменён напрямую; export/profile disk I/O пока синхронный. Отсутствие строки не доказывает OFFLINE.
- UPX для обоих macOS Mach-O проверен, но stable 5.2.1 сообщает «macOS is currently not supported». Принудительный режим не используется; universal и подпись сохранены, сжат DMG.

Скопируйте .app из образа перед запуском и разрешите доступ к локальной сети. В DMG включены README и Smoke-test.command; `--smoke-test` делает hidden own-IP integration, не выдавая непроведённые tests за passed. Подробности: [PORT_TESTING](https://github.com/nr084/Look-LAN-ng/blob/main/docs/PORT_TESTING.txt).

## English

First macOS and Ubuntu port preview, **cross-compiled locally on Windows**. Feature parity is incomplete; this is not a stable port release.

- macOS 13+: native Cocoa, universal Intel x86_64 + Apple Silicon arm64, real HFS+ DMG with LZMA. Ad-hoc signed, not notarized. Code-page, Info.plist/resource hashes, architectures, executable modes and complete DMG extraction round-trip verified. Native mount/launch/LAN validation is assigned to the user's Mac.
- Ubuntu 22.04+ Desktop x64: native GTK 3 with the shared C17 backend; ZIP and launcher. Latest stable UPX 5.2.1 --ultra-brute --lzma, integrity passed. Native runtime testing awaits the user's test machine.
- Implemented IPv4 NIC/range selection, source-bound background ICMP/TCP discovery, Stop/safe close, MAC cache enrichment, OUI, quick TCP services, table/filter/details, repeated-scan states, UTF-8 profiles, CSV/TXT/HTML and RU/EN/system language.
- Remote DNS/NetBIOS, standalone Ping/Trace, automatic monitoring, profile editor, remote context actions, device notes/history UI, statistics and full Windows layout are not ported yet. Windows fresh-ARP discovery has no direct replacement yet; export/profile disk I/O remains synchronous. An absent row does not prove OFFLINE.
- Stable UPX 5.2.1 rejects both macOS Mach-O targets as unsupported. No forced mode: universal architecture and signing are preserved; the DMG itself is compressed.

Copy the app out of the image before launching and allow local network access. README and Smoke-test.command are included. Native runtime tests have not been performed here.

## SHA-256

- `look@lan-ng-macos-universal-1.2.0-port.1.dmg`: `1c313aa374fc6d326d5128f8fc6871cd0c8106064c070405ae36ed4a9baccee4` (721125 bytes)
- `look@lan-ng-linux-x64-1.2.0-port.1.zip`: `65338c87f2ad4c82fc8eb2e27c470f74ae9674cb572c158b4eb2c4e87cce7b58` (323769 bytes)
