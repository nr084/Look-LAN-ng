# macOS native candidate — 1.3.1-rc.1, 13 October 2026

macOS builds and tests have moved to Mac by the user's explicit instruction. macOS cross-build/packaging on Windows is retired; Windows and Linux retain their Windows workflow. The earlier no-tests instruction is superseded for this Mac work.

Built here: macOS 27.0.1 (26A434), Apple Silicon arm64, Apple Clang 21.0.0, CMake 4.4.4. Release universal x86_64+arm64; deployment baseline macOS 13.0. Current source corrections are included in the Mac artifact; other-platform binaries were not rebuilt or tested in this session.

## Results / результаты

- Native arm64 CTest: **6/6 passed**, 9.90 seconds. All six suites also **passed under Rosetta x86_64**. Rosetta is not an Intel-machine test.
- Runtime suite covers a real source-bound UDP DNS fixture, parser/cache/generation checks, timed monitoring, four self-IP Ping replies, one-hop self-IP Trace, cancellation, immutable background export, UTF-8 profile save/open, cancelled export, missing-interface handling and 100 Scan/Stop cycles. Rosetta max Stop: 13 ms. FD=-1 and RSS=0 on macOS mean these measurements are unavailable; no Mac resource-leak conclusion is claimed.
- Final stripped/ad-hoc signed application's `--smoke-test`: exit 0 for arm64 and x86_64. This flag exercises backend/self-IP scan; the Cocoa GUI was checked separately.
- Cocoa GUI: actual startup/NIC discovery, full subnet scan, host selection, four Ping samples, self-IP Trace, live filtering and clearing, RU/English/system switching, profile editor opening/cancellation and Quit during active scan. Final exact packaged app completed **254/254 addresses, 11 ONLINE, 18.946 s**, without the network-error alert. GUI screenshot inspected locally; no release marketing screenshot is claimed.
- Original icon/localized network permission resources included. `codesign --verify --deep --strict` passed; DMG integrity/mount/file hashes/execute modes passed. ULMO/LZMA compression. No temporary mounted image remains.

## Fixes found by native checks

1. macOS routes traffic to the NIC's own IP through lo0. DNS, probes and Trace now retain source-IP binding without forcing local endpoints onto the physical interface. Before correction, the native DNS fixture failed; after correction, both architecture suites passed.
2. ICMP send errors for a target with no neighbor/route (EHOSTDOWN/EHOSTUNREACH/ENETUNREACH) are normal no-route probe results, not a failure invalidating the whole scan/monitoring round. Temporary buffer pressure maps to a timeout. A real full subnet pass previously showed Network error: 5; repeated passes after the correction completed without that alert.
3. Range-field placeholder now updates immediately with RU/EN selection.

## Exact local artifact

- DMG: `look@lan-ng-macos-universal-1.3.1-rc.1-native.dmg` (715666 bytes).
- SHA-256: `121f4728eb690a19f00a0eef9c29505518fcd939e9c473c8b0aa38828f34e2ba`.
- Signed executable SHA-256: `53bb7ccf66317ca243614ca17e9ca773377965fe381ca1ed19257cdcd71587c0`.
- App: `dist/1.3.1-rc.1/macos-native/Look@LAN-ng.app`.
- Raw diagnostic executable: `build/unpacked/1.3.1-rc.1/macos-native-universal/Look@LAN-ng`.
- Logs: `build/validation/macos-native/`; package metadata: `dist/macos-native-package.json`.

Latest stable UPX was checked against the official API: 5.2.1. macOS support remains disabled upstream (https://github.com/upx/upx/blob/v5.2.1/NEWS); no force packing is used. The Mach-O stays intact and the DMG is LZMA compressed.

## Limits / ограничения

This is a tested local RC, not a published stable release or a complete native validation matrix. Native Intel hardware, macOS 13 runtime, fresh-ARP/cache-only controlled fixtures, multi-hop Trace, real failure/NIC-unplug matrix, full GUI file-dialog/export/context-action coverage, physical display scaling and long resource/monitoring stress remain unverified on Mac. The bundle is ad-hoc signed, not Developer-ID notarized; Gatekeeper behavior of an internet-downloaded/quarantined artifact was not tested. The pre-existing VPN/network setup was retained. No credentials are recorded.

Temporary build/bootstrap/packaging/test files were removed; source, current artifacts, build dependencies and evidence are retained. The directory is a synced source snapshot without .git, so no commit/push was made.

---

## Historical Windows cross-build evidence (superseded)

# macOS local candidate — 1.3.0-rc.4

Built on Windows on 13 October 2026 for macOS 13+, universal x86_64/arm64.
DMG: `look@lan-ng-macos-universal-1.3.0-rc.4.dmg` (1576741 bytes).
SHA-256: `f4c01a8bab97e8744235b4fd315bf2430157b501384e19da44d037e3ecff420e`.
This candidate is local and has not been published as a GitHub release.

## Implemented / реализовано

- Commands are validated consistently during scan, monitoring, file jobs and
  shutdown. Modal return paths do not start work after the backend has closed.
- Network/range/filter widths adapt to the window. Scan table refreshes are
  coalesced at 150 ms while backend polling/cancellation remains at 50 ms.
  Selection restoration checks stale indices. Double-click opens General;
  diagnostics select Ping/Trace. Error alerts use the selected RU/EN language.
- Darwin traceroute checks pipe/spawn setup, uses nonblocking reads, closes
  owned descriptors, kills/reaps its own child on cancellation and rejects
  overflowing lines, non-finite latency and duplicate/out-of-range hops.
  Failures report a system error instead of silently succeeding.
- Numeric bundle build ID is generated from the release version and iteration.
  It follows Apple's [CFBundleVersion format](https://developer.apple.com/documentation/bundleresources/information-property-list/cfbundleversion).
- `--target macos` packaging preserves Linux assets/validation metadata.
  Repacking an already published Linux version is rejected before payload work.
- The candidate contains six signed universal native test programs. The command
  runs them in a temporary working directory/HOME, reports each exit status,
  preserves `Native-tests.log` beside the command and removes its own temp data.
  Exit 77 is a skipped check, never a pass. Final stable DMG excludes these tests.

## Checked locally / проверено локально

- Intel and Apple Silicon C/Objective-C cross-builds completed without errors.
- Application and all six test programs contain both architectures and have
  verified ad-hoc code page hashes; application Info.plist/resource seals match.
- LZMA HFS+ DMG extraction matches every payload hash and executable mode.
- Latest official stable UPX 5.2.1 was checked. Both Mach-O targets are rejected
  without force; executable packing is not applied. DMG uses LZMA compression.
- Published Linux ZIP still has SHA-256
  `743a896be8fe06118c0ec1ff4cf48ea492f14d19b2395003117f8b47523c5fc2`.

## Native checks pending / ожидают доступа к Mac

User will provide Mac access later. No new native success is claimed from
cross-compilation or the historical port.1 screenshot.

1. Mount exact DMG, copy payload to a writable location; verify native startup,
   local network permission and scan of real hosts on the selected interface.
2. Run all six tests and smoke test; retain architecture, OS and exit statuses.
   Test x86_64 under Rosetta separately on Apple Silicon if available; this does
   not substitute for an Intel-machine validation.
3. Exercise RU/EN/system defaults, menus, small/large windows, table sort/filter,
   selection and six details tabs; Unicode profile editor/save/open/remapping,
   cancelled dialogs and close during modal/file/network/diagnostic work.
4. Verify fresh ARP versus cache-only status, source-bound PTR/NBNS, automatic
   monitoring, actual Ping/traceroute and Stop responsiveness on this Mac.
5. Verify all export formats and atomic cancellation; take the final screenshot
   only from the eventual validated release. User supplies the Mac screenshot.

Native GUI, Gatekeeper/notarization policy, DMG mount, networking and traceroute
checks remain pending. The bundle is ad-hoc signed, not notarized. Release
publication requires evidence for the exact final artifact.
