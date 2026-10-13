# macOS 1.3.1 — native release validation

Built and tested locally on Mac, 13 October 2026. This release contains macOS only; Windows/Linux were not compiled in this workspace. Previous Mac candidates and cross-build evidence are archived in [MACOS_VALIDATION_HISTORY.md](MACOS_VALIDATION_HISTORY.md).

## Environment and artifact

- macOS 27.0.1 (26A434), Apple Silicon arm64, Apple Clang 21.0.0, CMake 4.4.4.
- Universal arm64+x86_64, deployment baseline macOS 13.0. x86_64 execution tested under Rosetta.
- Artifact: `look@lan-ng-macos-universal-1.3.1.dmg`, 716118 bytes.
- DMG SHA-256: `824493f5b2aee7572d44754019a9a8cadf239959464b17c491ba63f063517f34`.
- Signed executable SHA-256: `7cfefacd747c4c243f8b9c20fe87bcbeb94db3bcb888021c86bd8d5405c299bb`.
- Native ad-hoc hardened-runtime signing; `codesign --verify --deep --strict` passed. No Developer ID certificate/notarization.
- HFS+ ULMO/LZMA DMG: integrity, actual mount, resource/executable hash and mode roundtrip passed.
- Latest stable UPX 5.2.1 checked against GitHub API; macOS support disabled upstream, no force packing. Mach-O retained, DMG compressed.

## Automated checks

- **6/6 native arm64 CTest passed** (9.61 seconds), **6/6 x86_64 suites passed under Rosetta**.
- Final signed application smoke: **arm64 and x86_64 exit 0**. This flag is a backend/self-IP test; Cocoa GUI checks are separate below.
- Core, ports/vendor, monitor state transitions and bounded ring, UTF-8 profiles, malformed input rejection, reports/escaping/statistics, scoped notes/Ping ring and NBNS parser suites.
- Runtime: real source-bound UDP PTR fixture; DNS/cache/MAC/generation invalidation; unavailable NIC/remapping; timed monitoring; four self-IP Ping replies; one-hop self-IP Trace; prompt diagnostic cancellation; immutable background CSV/TXT/HTML files, cancelled export preserving target; Unicode profile Save/Open; 100 Scan/Stop cycles; close/idempotence.
- Runtime FD **3 → 3**. Native arm64 max Stop **13 ms**; Rosetta measurements are in its log.
- **120 scheduled monitoring rounds (~10 minutes)** on self IP and router: FD remained **3**, baseline RSS **11664 KiB**, maximum **11696 KiB**. This is bounded backend stress, not a claim of hours/days of GUI resource stability.

## Native Cocoa GUI checks

Actual release app, real network results, no sample host data:

- Startup, NIC/sidebar discovery, full /24 discovery (254/254 addresses), progressive rows, real names/services and selected-host details.
- All six tabs: General, Ports, Ping, Trace, History and complete Log. Statistics dialog.
- Four Ping samples, self-IP Trace; monitoring repeats at a five-second interval and Stop returns to idle.
- Single Host resolves `localhost`; live filter/clear, row selection and horizontal scrolling; RU/English/system language.
- Profile editor accepts Unicode, rejects invalid TCP port 0, then accepts corrected values. Native SavePanel/OpenPanel roundtrip with a Unicode filename; saved-profile sidebar updates.
- GUI export through native SavePanel: CSV, TXT and HTML to Unicode paths, files read back and checked. Dialog cancellation checked.
- Unicode per-device note persists and now appears immediately in General after Save.
- Quit during active scan checked. Native tests also cover safe close during backend work.
- Real final RU/EN screenshots: docs/images/macos-1.3.1-ru.png and macos-1.3.1-en.png. These are release app captures, not mockups.

The exact final screenshot-session discovery count/time is recorded in local GUI evidence. Discovery count may vary between passes as real hosts respond.

## Corrections delivered

- Own-IP DNS/probes/Trace retain source binding without forcing loopback traffic onto the physical NIC.
- ICMP no-route/host-down results no longer invalidate the entire scan/monitoring round. Buffer pressure is a timeout, not fabricated reachability.
- RU/EN range placeholder changes immediately.
- Saved per-device notes refresh General immediately.
- macOS build/packaging workflow is native on Mac, with resources, raw diagnostics and checked LZMA DMG. GitHub publication checks exact hashes before exposing a stable release.

## Limits recorded for this release

Intel hardware and older macOS versions were not available; Rosetta is not an Intel-machine test. macOS 13 is a deployment baseline, not a tested runtime. Fresh-ARP/cache-only controlled fixtures, physical NIC unplug/display scaling, live non-ASCII NBNS, multi-hop external Trace, all external system-client actions and hours/days monitoring were not tested on this Mac. No internet-download quarantine/Gatekeeper/notarization success is claimed. Existing VPN/network configuration was retained. Optional mDNS, active IPv6 and persistent disk device history remain outside the mandatory scope.

Evidence: build/validation/macos-native/; raw binary: build/unpacked/1.3.1/macos-native-universal/. Temporary export/profile/packaging files are removed after verification; settings and user profiles are retained. Diagnostics ZIP retains raw builds and automated logs. No credentials are stored in source or release artifacts.

## Publication verification

Published as latest stable: https://github.com/nr084/Look-LAN-ng/releases/tag/v1.3.1 . DMG and diagnostics ZIP downloaded back from GitHub; SHA-256 matches the local native-validated artifacts. Both screenshot PNGs match the bytes in the release tag. Source commit: e573a1d8abcdca076f9e793284b5d629ecfa6ef0. The superseded empty draft created during publisher setup was removed; existing Windows/Linux release history retained.
