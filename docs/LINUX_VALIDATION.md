# Linux native validation — 13 October 2026

Compilation and packaging took place locally on Windows. The provided VM runs Ubuntu 26.04.1 LTS x86_64. All scanner/test executables ran as an ordinary user. Root created and removed only owned veth/network-namespace fixtures; no remote compilation was used. GUI tests use isolated D-Bus/Xvfb/Openbox and separate configuration directories.

The current **local candidate 1.3.0-rc.2** is not a published stable release.

| Evidence | Result |
| --- | --- |
| Five portable C suites | Passed: core, features, monitor, profile/report, details/NBNS |
| Native POSIX integration | Passed: selected-source DNS fixture and parser, name cache/MAC invalidation, stale generations, missing-interface protection/remapping, timed monitoring, history/statistics/notes, four Ping replies, one-hop Trace, diagnostic cancellation, async snapshot/export/profile/cancellation |
| 100 Scan/Stop cycles | Passed; final run FD 6 → 6, maximum Stop 11 ms |
| Close/idempotence | Passed; bounded under 3 s |
| Fresh ARP-only host | Passed without root: namespace drops all ICMP/TCP/UDP input; fresh ARP still confirms ONLINE with MAC and no fake latency |
| Static neighbor cache | Passed: cached MAC alone stays UNKNOWN, with no first_seen/ONLINE evidence |
| Raw ELF | PIE (DYN), GNU_RELRO+BIND_NOW, RW non-executable GNU_STACK |
| UPX | Stable 5.2.1; official digest verified; --ultra-brute --lzma; upx -t passed |
| Final ZIP/modes | SHA-256 and binary bytes matched on VM; execute modes preserved |
| Packed own-IP scan | Passed |
| Packed GTK workflows | 38/38 passed using the exact binary identified below |
| Windows x64 regression | All 15 test cases passed: 14 in sandbox and TCP backend separately outside restricted network sandbox (43.24 s) |

Final Linux ZIP SHA-256: `d6fc81385b89363d7e6acc4695bcf97f7d58fd880be9d323e98b1128317dffb6`.
Packed executable SHA-256: `cc4de1a920775d2015ed727b0cf6e49bd46526b20c7e3b87c74d322bb92f78cd`.
The final README update changed the archive hash; a byte-for-byte comparison on VM confirmed the binary was identical to the 38/38 GUI-tested executable, then packed own-IP smoke was repeated.

The GUI suite covers real own-IP/name/MAC and gateway, scan deduplication, filter, CSV/TXT/HTML and uppercase HTML, save/open profile and saved-profile sidebar, multiple ranges/editor, six tabs, Ping/Trace, notes/history/statistics, Copy IP/hostname clipboard, monitoring/Stop, hostname Single Host with retained profile ranges, explicit interface remapping, invalid range recovery, F5 during scan, bounded close, immediate and persisted RU/EN/system-language mapping.

Local diagnostic reports: build/validation/linux-rc2-native.log, linux-rc2-packed-gui.log, linux-rc2-gui-results.json, linux-rc2-window.png and dist/linux-candidate-validation.json. The image is a private validation capture, not the final GitHub screenshot. Owned virtual network fixtures were removed after the tests; temporary GUI/configuration directories are cleaned after fetching results.

Reproduce GUI tests against an already cross-built application:

```sh
dbus-run-session -- python3 tools/test-linux-gui.py --binary /path/to/LookLAN.sh
```

Dependencies: GTK 3, python3-pyatspi, python3-xlib, python3-pil, Xvfb, xdotool, Openbox. The driver creates its own GUI session and temporary configuration, and performs short own-subnet and gateway scans. Native tests are tests/posix_runtime_tests.c, compiled on Windows by tools/build-ports-windows.py.

Ubuntu 22.04 remains a compilation baseline, not a verified runtime. Physical NIC unplug, physical HiDPI displays and hours-long LAN stress are not represented by these VM tests. Missing-interface behavior and two-miss/state logic are checked independently. Locale mapping uses environment locales, not a separately installed Russian desktop. NBNS wire parsing is tested; a live Windows NBNS server/non-ASCII OEM conversion is not claimed. Non-ASCII OEM names remain unknown; mDNS/active IPv6 and persistent disk history are outside current scope.

macOS: the user confirmed old port.1 startup and real discovery. New universal rc.2, Cocoa layout, routing events, Ping/Trace and full workflows still require user native results. The final DMG integrity report is dist/ports-package.json. Neither port candidate is uploaded to GitHub; stable publication requires exact-artifact native validation and final release screenshots.
