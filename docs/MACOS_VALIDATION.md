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
