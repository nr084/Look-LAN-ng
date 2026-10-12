# Linux native validation — 12 October 2026

Compilation and packaging were performed locally on Windows. The user's SSH VM ran Ubuntu 26.04.1 LTS x86_64; tests ran as an ordinary user without root privileges. GTK 3, Xvfb, Openbox, D-Bus and AT-SPI supplied the isolated GUI test session. Openbox and its dependencies were installed on the VM for testing. No remote compilation was used.

## Results / Результаты

- Five portable C test executables passed: core, features, monitor, profiles/reports and details/NBNS. These cover simulated discovery, state transitions, parsing and escaping; they are not live NetBIOS or network-loss tests.
- The original published port.1 ZIP SHA-256 matched `65338c87f2ad4c82fc8eb2e27c470f74ae9674cb572c158b4eb2c4e87cce7b58`. Its UPX-packed application and the raw binary both passed native own-IP smoke tests.
- The corrected port.2 UPX build passed startup and real scans of the VM's own IPv4 and its gateway. The gateway returned ICMP latency, MAC/vendor enrichment and TCP service results. Discovery did not need root.
- The new POSIX integration test passed scan/rescan, invalid-range recovery, 100 Scan/Stop cycles and close/idempotence. File descriptors remained **6 → 6**; maximum Stop measured **11 ms** in this run. This is a bounded own-IP stress check, not a long-duration LAN leak test.
- **23 GTK GUI checks** passed: English system locale; own IP/name/MAC; deduplication; filtering; CSV/TXT/HTML exports including uppercase `.HTML`; save/open profiles; gateway; invalid range; F5 while scanning; Stop; close while scanning with exit 0 under 3 seconds; immediate RU switch; idle close; saved language overrides and Russian system-locale selection. Export contents and the saved profile/settings were also read back and checked.
- System-language mapping was exercised through environment locale values. A Russian desktop installation and physical DPI/monitor configurations were not tested.
- Windows x64 regression after the shared TTL correction: **15/15 CTest passed, 52.74 s**.

Reproduce GUI tests on Linux using the locally cross-built binary:

```sh
GTK_MODULES=gail:atk-bridge dbus-run-session -- python3 tools/test-linux-gui.py --binary /path/to/LookLAN.sh
```

Dependencies: `python3-pyatspi`, `python3-xlib`, `python3-pil`, `xvfb`, `xdotool`, `openbox`, GTK 3. The script creates its own Xvfb/D-Bus session, configuration and artifact directory. Tests include short scans of the selected local IPv4 subnet and the default gateway.

## Fixes / Исправления

F5 during a running Linux scan previously opened an empty error dialog; the GUI test reproduced this on port.1. The scan callback now ignores requests while scanning, closing or without a selected interface. Uppercase export extensions now select the expected report format. Linux toolbar captions are retained alongside symbols and translated tooltips describe shortcuts. Shared OS guessing requires an actual nonzero TTL before combining it with SSH/latency to suggest Linux; unknown TTL no longer acts as Linux evidence. A portable regression covers unknown versus known TTL.

## Release gate / Условия публикации

The implemented Linux workflows are tested, but the port still lacks remote DNS/NetBIOS, standalone Ping/Trace, automatic monitoring, profile editor, remote context actions, device notes/history UI and statistics. Export/profile file I/O is synchronous. Ubuntu 22.04 is the compilation baseline, not a tested runtime environment. Interface loss, ICMP-blocked real hosts, stale-cache live fixtures and hours-long stress remain outside this run.

По последнему указанию пользователя новые Linux preview artifacts не публикуются. Завершение проверки текущих функций не означает полную готовность по ТЗ. Linux stable остаётся закрыт до завершения портирования и соответствующих тестов.

macOS: the user confirmed port.1 startup and real LAN discovery with a screenshot. Mac version/CPU and the full native test matrix were not provided; do not count that report as two-architecture validation. port.2 needs its own native user test.
