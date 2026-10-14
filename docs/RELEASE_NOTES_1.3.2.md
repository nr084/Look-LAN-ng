# Look@LAN-ng 1.3.2 — Windows / Ubuntu / macOS

## Русский

Теперь один релиз содержит Windows x86/x64/ARM64, Linux x64 и macOS universal.
Windows/Linux собраны локально на Windows; опубликованная macOS сборка создана
отдельно на Mac и сохранена без замены. Native Win32 / GTK3 / Cocoa, RU/EN.

Реальное IPv4-сканирование ARP/ICMP/TCP, DNS/NetBIOS, MAC/OUI, monitoring,
Ping/Trace, профили и CSV/TXT/HTML export. Устройства появляются постепенно,
Стоп отменяет работу. История и заметки устройства — текущей сессии.

Windows ZIP: один EXE своей архитектуры. Linux ZIP: ./LookLAN.sh и системные
GTK 3 libraries. Mac: DMG → Applications, macOS 13+ deployment baseline.
Mac подпись ad-hoc, без notarization. Windows/Linux максимально сжаты UPX 5.2.1,
integrity и запуск проверены; Mach-O сохранён, DMG сжат ULMO/LZMA.

Windows x86/x64: по 15 CTest и четыре packed smoke modes. ARM64: 11 C suites
и четыре packed smoke modes на нативном Windows 11 ARM64 runner, без сборки
на GitHub. Ubuntu 26.04.1: шесть C suites, 39 GUI workflows, packed startup,
100 Scan/Stop cycles, FD 6 → 6, максимальный Stop 12 ms. Mac: native arm64,
Rosetta x86_64, app smoke, GUI и LAN scan — результаты отдельной Mac-сборки.

Полная Windows 10/11/physical DPI/NIC/длительная matrix, Ubuntu 22.04 runtime,
Intel hardware и старые macOS не объявляются проверенными. Rosetta не заменяет
Intel hardware. [Windows/Linux evidence](https://github.com/nr084/Look-LAN-ng/blob/main/docs/WINDOWS_LINUX_VALIDATION_1.3.2.md)
· [Mac evidence](https://github.com/nr084/Look-LAN-ng/blob/v1.3.2/docs/MACOS_VALIDATION.md).

## English

One release now provides Windows x86/x64/ARM64, Linux x64 and macOS universal.
Windows/Linux are locally built on Windows; the separately built and validated
Mac artifacts are preserved. Native UI, RU/EN, progressive discovery, monitoring,
diagnostics, profiles and background CSV/TXT/HTML export; no cloud or telemetry.

Extract the Windows EXE, run ./LookLAN.sh on Linux with system GTK 3, or drag
the Mac application from its DMG into Applications. Maximum stable UPX packing
and native startup checks passed on Windows/Linux. Mac is ad-hoc signed without
notarization, with ULMO/LZMA DMG compression rather than forced Mach-O packing.

Windows x86/x64 each passed 15 CTest and four packed smoke modes. Native ARM64
passed eleven locally compiled C suites and four packed smoke modes in a
runtime-only GitHub job. Ubuntu passed six native suites and 39 GUI workflows;
100 Scan/Stop cycles retained six FDs, with maximum Stop of 12 ms. Mac arm64,
Rosetta x86_64, signed-app smoke and actual LAN checks are separately documented.
These are not a full hardware/OS/NIC/DPI/long-load matrix; older OS versions and
Intel Mac hardware remain unverified. Device notes/history are session-only.

## Actual screenshots / Реальные скриншоты

Final builds after real LAN scans, not sample data. IP addresses are blurred for privacy. / IP-адреса скрыты размытием.

![Windows 1.3.2 RU](https://raw.githubusercontent.com/nr084/Look-LAN-ng/main/docs/images/windows-1.3.2.png?privacy=20261014)

![Ubuntu 1.3.2 EN](https://raw.githubusercontent.com/nr084/Look-LAN-ng/main/docs/images/linux-1.3.2.png?privacy=20261014)

![macOS 1.3.2 RU](https://raw.githubusercontent.com/nr084/Look-LAN-ng/main/docs/images/macos-1.3.2-ru.png?privacy=20261014)

[Mac English screenshot](https://raw.githubusercontent.com/nr084/Look-LAN-ng/main/docs/images/macos-1.3.2-en.png?privacy=20261014)

## SHA-256

| File | SHA-256 |
| --- | --- |
| `look@lan-ng-x86-1.3.2.zip` | `ab4f2f0f3e44b7b9041f20cafb50236ce513f5339f9820a5004d462f85606c6c` |
| `look@lan-ng-x64-1.3.2.zip` | `dfa4dae62778e9d0c5ff16085acc8121f4242ee454cd180d4ca152772c6703f7` |
| `look@lan-ng-arm64-1.3.2.zip` | `e438ee62294bbfc79859cc7f924f8019cf2b7214671621b9c27b9609a0bb8015` |
| `look@lan-ng-linux-x64-1.3.2.zip` | `cc2fcc0b3591421a5a74c25883199299396071679f7d309a7be7081f0efa0132` |
| `look@lan-ng-macos-universal-1.3.2.dmg` | `6ed919c223c43e0ee4d5962816e170693be089b7d7f027b9ea81d33487084a3c` |

