# Linux stable 1.3.0 — 13 October 2026

Final locally built Windows→Linux ZIP: `look@lan-ng-linux-x64-1.3.0.zip`, SHA-256 `743a896be8fe06118c0ec1ff4cf48ea492f14d19b2395003117f8b47523c5fc2`. Tested as an ordinary user on Ubuntu 26.04.1 x64 VM: six native suites and **39/39 final packed GTK workflows**, including the full native log table and actual Close via the window manager. Own-IP packed smoke and real default-subnet discovery passed. The final screenshot shows **11 actually discovered hosts**; it is not a sample fixture.

**120 scheduled monitoring rounds (~10 minutes)** on own IP and gateway: FDs **6 → 6**, RSS **11548 → max 11680 KiB**, within the expected bounded session-history storage. 100 Scan/Stop checks: maximum Stop **11 ms**, close/idempotence passed. Compiler warnings eliminated; src contains no strcpy/sprintf. PTR CNAME/length validation, sparse 4000-port round-trip, 64 KiB profile writer bounds, source/MAC snapshot locking and configured worker cap are tested. PIE/full RELRO/NX raw ELF; stable UPX 5.2.1 --ultra-brute --lzma integrity passed.

Latest evidence: dist/linux-release-validation.json; build/validation/linux-1.3.0-native.log, linux-1.3.0-gui.log, linux-1.3.0-gui-results.json, linux-monitoring-stress.log. Screenshot: docs/images/linux-1.3.0.png. The earlier GTK test driver sometimes targeted hidden leader windows when sending WM_DELETE; it now activates and closes the exact process window through the window manager. This is a test-driver correction, not proof of an application close failure.

Windows regression: all 15 cases passed, with the affected profile/UI/file checks repeated after the discovered 4096-byte port-parser limit was fixed. Actual Windows screenshot is taken from published packed **1.2.0**, distinct from the new Linux version.

Ubuntu 22.04 is a cross-build baseline, not a verified runtime. Physical HiDPI/NIC unplug and hours-long LAN/GUI stress are untested. The ten-minute monitoring check is a backend/controller test, not a days-long GUI test. Non-ASCII OEM NBNS names remain unknown; mDNS/active IPv6 are future scope. macOS rc.3 requires native user validation and is withheld from GitHub.

The historical candidate evidence below describes preceding builds; use the hashes/results above for the stable release.

---

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
