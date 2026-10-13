# Look@LAN-ng 1.3.2 — macOS

## Русский

Обновлённый интерфейс macOS: таблица и подробности занимают доступную ширину, исправлена компоновка восстановленного и изменённого окна, убран лишний отступ сверху. Действия получили подписи, вкладки подробностей стали компактнее, пустая таблица показывает подсказку. Ping/Trace доступны сразу после выбора устройства.

Universal Apple Silicon + Intel, native Cocoa, RU/EN. Собрано и проверено на Mac: 6/6 arm64 тестов, шесть x86_64 тестов через Rosetta, оба smoke-теста подписанного приложения, реальное сканирование и проверка интерфейса. Windows 1.2.0 и Linux 1.3.0 сохранены в истории.

Откройте DMG и перенесите приложение в Applications. Минимальная цель сборки — macOS 13; старые macOS и Intel hardware не проверены. Подпись ad-hoc, без notarization. UPX 5.2.1 отключает поддержку macOS: Mach-O сохранён, DMG максимально сжат ULMO/LZMA. [Проверки и ограничения](https://github.com/nr084/Look-LAN-ng/blob/v1.3.2/docs/MACOS_VALIDATION.md).

## English

Updated macOS interface: the host table and details fill the available width, restored/resized window layout is fixed, and the excessive top gap is removed. Native toolbar actions have labels, detail tabs are compact, and empty tables show guidance. Ping/Trace become available immediately after selecting a host.

Universal Apple Silicon + Intel, native Cocoa, RU/EN. Built and checked on Mac: six arm64 tests, six x86_64 tests under Rosetta, both signed-app smoke modes, actual network discovery and UI checks. Existing Windows 1.2.0 and Linux 1.3.0 remain in history.

Open the DMG and drag the app to Applications. macOS 13 is the deployment baseline; older macOS and Intel hardware were not tested. Ad-hoc signed, not notarized. UPX 5.2.1 disables macOS packing: Mach-O is retained and the DMG uses ULMO/LZMA compression. [Validation and limits](https://github.com/nr084/Look-LAN-ng/blob/v1.3.2/docs/MACOS_VALIDATION.md).

## Actual screenshots / Реальные скриншоты

![macOS 1.3.2 RU](https://raw.githubusercontent.com/nr084/Look-LAN-ng/v1.3.2/docs/images/macos-1.3.2-ru.png)

![macOS 1.3.2 EN](https://raw.githubusercontent.com/nr084/Look-LAN-ng/v1.3.2/docs/images/macos-1.3.2-en.png)

## SHA-256

| File | SHA-256 |
| --- | --- |
| `look@lan-ng-macos-universal-1.3.2.dmg` | `6ed919c223c43e0ee4d5962816e170693be089b7d7f027b9ea81d33487084a3c` |
| `look@lan-ng-macos-diagnostics-1.3.2.zip` | `09698c323da3a5eb5afff04e8742de7dabe79b84af6c9aec7e0e2552928ff533` |

DMG: application. ZIP: optional raw diagnostics and test logs. No cloud, accounts or telemetry. © nr084, 2026 — year generated during the build.
