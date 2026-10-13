# Look@LAN-ng 1.3.1 — macOS

## Русский

Первый stable macOS-релиз: native Cocoa, universal Apple Silicon + Intel, русский и английский интерфейс. Выпуск содержит только macOS; Windows 1.2.0 и Linux 1.3.0 сохранены в истории.

- IPv4 discovery, MAC/vendor, DNS/NetBIOS, TCP-порты, Single Host, фильтр и сортировка.
- Мониторинг, история/журнал сессии, статистика, отдельные Ping/Trace и заметки устройств.
- UTF-8 профили, background CSV/TXT/HTML export и штатные Mac-диалоги.
- Исправлены собственный IP/loopback, обработка недоступных адресов, перевод подсказки диапазона и немедленное обновление заметок в «Общее».

Собрано и проверено локально на macOS 27.0.1 / Apple Silicon: 6/6 arm64, 6/6 Rosetta x86_64 suites, оба smoke modes финального app и реальные Cocoa/LAN проверки. Последний проход: 254 адреса, 9 активных узлов, 18.928 с. 120 раундов мониторинга (~10 минут): FD=3, RSS 11664 → max 11696 KiB. Подпись, DMG integrity/mount/hash/modes прошли.

Откройте DMG и перенесите приложение в Applications. Deployment baseline — macOS 13; старые macOS и Intel hardware не проверялись, Rosetta не заменяет Intel-тест. Подпись ad-hoc, без Developer ID/notarization. Полная NIC/DPI/длительная runtime matrix не заявляется пройденной. [Подробные результаты](https://github.com/nr084/Look-LAN-ng/blob/v1.3.1/docs/MACOS_VALIDATION.md).

## English

The first stable macOS release: native Cocoa, universal Apple Silicon + Intel, Russian and English interfaces. macOS only; Windows 1.2.0 and Linux 1.3.0 remain in release history.

- IPv4 discovery, MAC/vendor, DNS/NetBIOS, TCP ports, Single Host, filtering and sorting.
- Monitoring, session history/log, statistics, separate Ping/Trace and device notes.
- UTF-8 profiles, background CSV/TXT/HTML export and native Mac file dialogs.
- Fixed own-IP/loopback traffic, unreachable-target handling, range-placeholder translation and immediate General refresh after saving notes.

Built and tested locally on macOS 27.0.1 / Apple Silicon: six arm64 and six Rosetta x86_64 suites, both final-app smoke modes and actual Cocoa/LAN workflows passed. Final discovery: 254 addresses, nine online hosts, 18.928 s. A 120-round monitoring check (~10 minutes) kept FD at 3; RSS 11664 → max 11696 KiB. Signature and DMG integrity/mount/hash/modes passed.

Open the DMG and drag the app to Applications. macOS 13 is the deployment baseline; older macOS and Intel hardware were not tested, and Rosetta is not an Intel-machine test. Ad-hoc signed, without Developer ID/notarization. Full NIC/DPI/long-runtime validation is not claimed complete. [Detailed evidence](https://github.com/nr084/Look-LAN-ng/blob/v1.3.1/docs/MACOS_VALIDATION.md).

## Actual screenshots / Реальные скриншоты

![macOS 1.3.1 RU](https://raw.githubusercontent.com/nr084/Look-LAN-ng/v1.3.1/docs/images/macos-1.3.1-ru.png)

![macOS 1.3.1 EN](https://raw.githubusercontent.com/nr084/Look-LAN-ng/v1.3.1/docs/images/macos-1.3.1-en.png)

## Assets / Поставка

The DMG is the app package; the ZIP contains optional raw diagnostic binaries and automated logs. UPX 5.2.1 disables macOS packing, so Mach-O is preserved and DMG uses ULMO/LZMA. No cloud, accounts or telemetry.

## SHA-256

| File | SHA-256 |
| --- | --- |
| `look@lan-ng-macos-universal-1.3.1.dmg` | `824493f5b2aee7572d44754019a9a8cadf239959464b17c491ba63f063517f34` |
| `look@lan-ng-macos-diagnostics-1.3.1.zip` | `63067b5c2fbcb00ee6269ea440f69e1704a8169ea5f5c48ba7ba4ea0b4fe40d9` |

© nr084, 2026 — copyright year generated during this build.
