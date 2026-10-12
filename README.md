# Look@LAN-ng

**RU:** Компактный портативный LAN-сканер для Windows x86, x64 и ARM64. Русский и английский интерфейс, современный native Win32 с лёгкой ретро-отсылкой.

**EN:** A compact portable LAN scanner for Windows x86, x64 and ARM64. Russian and English interfaces, modern native Win32 UI with a subtle retro influence.

[Релизы / Releases](https://github.com/nr084/Look-LAN-ng/releases) · [ТЗ / Specification](docs/SPECIFICATION.md) · [Статус / Status](docs/SPEC_STATUS.md) · [Build](BUILD.md)

## Русский

### Скачать и запустить

В каждом релизе — три отдельных ZIP, каждый содержит один `Look@LAN-ng.exe`:

| Архитектура Windows | Файл |
| --- | --- |
| x86, 32-битная | `look@lan-ng-x86-1.2.0.zip` |
| x64, 64-битная Intel/AMD | `look@lan-ng-x64-1.2.0.zip` |
| ARM64 | `look@lan-ng-arm64-1.2.0.zip` |

Скачайте ZIP своей архитектуры со страницы Releases, распакуйте, запустите EXE. Установка, драйверы, права администратора и дополнительные runtime не нужны. Выберите сетевой адаптер и нажмите **Сканировать / F5**. **Стоп / Esc** отменяет операцию.

Язык: **Вид → Язык** → **Язык системы (по умолчанию)**, **English**, **Русский**. Русский язык интерфейса Windows выбирает русский, любой другой — английский. Ручной выбор сохраняется. Настройки portable записываются рядом с EXE; если папка недоступна для записи — в LocalAppData.

### Возможности

- Обнаружение IPv4 через ARP, ICMP и TCP с выбранного интерфейса; результаты появляются постепенно.
- DNS и NetBIOS-имена, MAC, встроенная OUI-база производителей, TCP-порты и осторожные подсказки ОС.
- Один узел, произвольные порты, фильтрация и сортировка; Web/HTTPS/RDP/SSH/SMB и копирование реквизитов.
- Мониторинг ONLINE/OFFLINE/NEW/CHANGED/UNKNOWN, журнал, статистика, отдельные Ping и Trace, заметки устройств.
- UTF-8 профили, CSV/TXT/HTML экспорт, RU/EN интерфейс и DPI 100–200%.
- C17, static CRT, только системные Windows API; без облака, аккаунтов, telemetry и фоновой службы.

Запись ARP-кэша сама по себе не означает ONLINE. OS hints не являются точным fingerprinting. История/заметки устройства сохраняются только в текущей сессии; профильные заметки сохраняются в профиле. SSH требует установленного Windows OpenSSH client. Активное IPv6-сканирование отсутствует; Linux/macOS остаются будущими платформами.

Полная ручная Windows 10/11, physical DPI/NIC, network-failure и длительная LAN-monitoring matrix ещё не выполнена. Автоматические тесты и UPX smoke checks описаны в [RELEASE_CHECK](docs/RELEASE_CHECK.md); они не заменяют эту matrix.

### Сборка и релизы

Visual Studio Build Tools 2022, C++ toolchain нужной архитектуры, Windows SDK, CMake. В PowerShell:

```powershell
./tools/build-windows.ps1 -Architecture x86
./tools/build-windows.ps1 -Architecture x64
./tools/build-windows.ps1 -Architecture arm64
```

GitHub Actions собирает и тестирует каждую архитектуру, максимально сжимает latest stable UPX (`--ultra-brute --lzma`), проверяет integrity и packed EXE. Push тега `v<версия>` публикует один релиз с тремя отдельными ZIP. SHA-256 ZIP включаются в описание релиза. Версия CMake, manifest, release notes и tag должны совпадать.

## English

### Download and run

Each release provides three separate ZIP files, each containing one `Look@LAN-ng.exe`:

| Windows architecture | File |
| --- | --- |
| x86, 32-bit | `look@lan-ng-x86-1.2.0.zip` |
| x64, 64-bit Intel/AMD | `look@lan-ng-x64-1.2.0.zip` |
| ARM64 | `look@lan-ng-arm64-1.2.0.zip` |

Download the ZIP matching your architecture from Releases, extract it and run the executable. No installation, drivers, administrator privileges or additional runtimes are required. Select a network adapter and press **Scan / F5**. **Stop / Esc** cancels the operation.

Language: **View → Language** → **System language (default)**, **English**, **Русский**. Russian Windows UI selects Russian; every other language selects English. Manual selection is saved. Portable settings are stored next to the executable, with a LocalAppData fallback if the folder is not writable.

### Features

- IPv4 discovery using source-bound ARP, ICMP and TCP; incremental results.
- DNS and NetBIOS names, MAC addresses, embedded OUI vendor lookup, TCP ports and cautious OS hints.
- Single-host and custom-port scans, filtering and sorting; Web/HTTPS/RDP/SSH/SMB and copy actions.
- ONLINE/OFFLINE/NEW/CHANGED/UNKNOWN monitoring, event log, statistics, standalone Ping and Trace, device notes.
- UTF-8 profiles, CSV/TXT/HTML export, RU/EN interface and DPI 100–200%.
- C17, static CRT and system Windows APIs only; no cloud, accounts, telemetry or background service.

An ARP cache entry alone does not establish ONLINE status. OS hints are not exact fingerprinting. Device notes and history are session-only; profile notes persist in the profile. SSH requires Windows OpenSSH client. Active IPv6 scanning is absent; Linux/macOS are future platforms.

The complete manual Windows 10/11, physical DPI/NIC, network-failure and long-running LAN monitoring matrix remains untested. See [RELEASE_CHECK](docs/RELEASE_CHECK.md) for automated tests and UPX smoke checks; these do not replace the manual matrix.

### Build and release

Use Visual Studio Build Tools 2022 with the target C++ architecture toolchain, Windows SDK and CMake. Run `tools/build-windows.ps1 -Architecture x86`, `x64` or `arm64` in PowerShell.

GitHub Actions builds and tests each architecture, packs it using the latest stable UPX with `--ultra-brute --lzma`, and checks integrity and packed executable startup. Pushing a `v<version>` tag publishes one release with three separate architecture ZIPs. ZIP SHA-256 checksums appear in the release notes. CMake, manifest, release notes and tag versions must match.

© nr084, build year / год сборки. Original Look@LAN code, logo and assets are not used.
