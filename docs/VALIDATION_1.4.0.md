# Validation 1.4.0 / Проверки

Windows/Linux release candidate, 30 October 2026. All compilation occurred locally on Windows. macOS is independently built, tested and published on Mac; no macOS 1.4.0 validation is claimed here.

| Final artifact | Actual verification |
| --- | --- |
| Windows x64 | 17/17 CTest; four packed smokes; 25 real GUI profile/theme lifecycle checks |
| Windows x86 | 17/17 CTest; four packed smokes on Windows 11 x64 |
| Windows ARM64 | 13 native C suites and four packed smokes on Windows 11 ARM64, OS 26200 |
| Linux x64 | Nine native suites, 39 GTK workflows, packed startup and actual LAN scan on Ubuntu 26.04.1 x64 VM |

[Native ARM64 runtime-only evidence](https://github.com/nr084/Look-LAN-ng/actions/runs/41058601241). Actions does not compile binaries. Native report SHA matches the final packed ARM64 EXE.

Windows GUI exercised temporary scans without INI/directories, native Save Profile, profile v2/history creation, singleton autoload, continued event sequence, restored Scan availability, clean close, and System/Dark/Light preference persistence. DPI smoke cycles all three themes, both languages and dialogs through 100 transitions with resource checks. The native menu/toolbar seam is painted in the selected palette; the white separator was removed and inspected in the final build.

Linux: persistent profile/history roundtrip, interface remapping, exports, diagnostics, monitoring, RU/EN overrides, cancellation and close. 100 Scan/Stop cycles: file descriptors 6 → 6, maximum Stop 11 ms. The packed executable extracted from the final ZIP matches the tested SHA-256.

Build tools verified stable LLVM 23.1.4 / LLVM-MinGW 20261020, IEEE MA-L 40421 prefixes, stable UPX 5.3.0 (`--ultra-brute --lzma`). UPX integrity and final packed startup checks passed. PE machine types, ASLR/NX, 64-bit HIGHENTROPYVA and absent GUARD_CF were verified. Each Windows ZIP contains one EXE. Linux executable mode is retained.

Limits: Windows 10 and a full Windows 11 hardware matrix, physical DPI/multiple NIC combinations, extended soak testing and older Linux distributions were not validated. Automated DPI transitions do not replace physical displays. OS-managed standard dialogs can follow the OS appearance. HISTORY rows describe previous observations until reprobed. This release does not claim optional active IPv6/mDNS support, persistent per-device notes or individual Ping/Trace samples. Profile v1 migration is intentionally absent.

RU: Проверены реальные финальные упакованные файлы, сохранение профиля и истории, отсутствие INI при временном запуске и переключение тем. Перечисленные ограничения остаются непроверенными; полная аппаратная матрица не заявляется. Mac проверяется отдельно.

## Screenshots / Скриншоты

Final real Windows/Ubuntu LAN scans. IP and MAC addresses are blurred using the built-in imagegen edit tool: preserve the captured native interface, blur every IPv4/CIDR/range/gateway/DNS/table/details address and every MAC address, preserve other labels and layout. These are privacy-edited screenshots, not pixel-exact captures. Saved to `docs/images/windows-1.4.0.png` and `docs/images/linux-1.4.0.png`. Older tracked screenshots are removed; source Git history is not rewritten.

## SHA-256

| ZIP | SHA-256 |
| --- | --- |
| `look@lan-ng-x86-1.4.0.zip` | `620fcedf2ddf88171fbbef81a61735d423cd51fe23b43a748e5a3f3f08fb0cb7` |
| `look@lan-ng-x64-1.4.0.zip` | `dfbe6dbf231e5412743a83fd80dc8633864e2366689fc9f1618e0ab90190ecde` |
| `look@lan-ng-arm64-1.4.0.zip` | `40587592d03377fe7dc88195b410f7830f6a2d4b8e38fad7d6a08e526bb8a005` |
| `look@lan-ng-linux-x64-1.4.0.zip` | `25286f85cc33a0db0ac5baced923c5bda8e77b19634ac6cb26c230c10e2ea87a` |

Local detailed evidence remains under `build/validation`: final CTest/build/packing logs, native ARM report, Linux native/GTK results, and `windows-1.4.0-profile-theme-results.json`. Private addresses and credentials are not included in this document.
