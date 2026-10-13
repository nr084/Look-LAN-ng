# macOS 1.3.2 — native release validation

Built locally on Mac, 14 October 2026. macOS only; this workspace does not build Windows/Linux. Previous release evidence is in [MACOS_VALIDATION_HISTORY.md](MACOS_VALIDATION_HISTORY.md).

## Build and package

macOS 27.0.1 (26A434), Apple Silicon, Apple Clang 21.0.0, CMake 4.4.4. Universal arm64+x86_64, deployment baseline macOS 13. Ad-hoc hardened-runtime signature verified; no Developer ID/notarization. HFS+ ULMO/LZMA DMG integrity, actual mount, file hashes and modes passed. Latest stable UPX 5.2.1 checked through the official GitHub API: macOS support is disabled upstream; Mach-O remains unpacked.

- DMG: `look@lan-ng-macos-universal-1.3.2.dmg`, 722986 bytes.
- DMG SHA-256: `6ed919c223c43e0ee4d5962816e170693be089b7d7f027b9ea81d33487084a3c`.
- Signed executable SHA-256: `902eb1f09a3c5187bc58bf87c1461c6b338f6bedcf105d3bda2433ac5d753a1d`.
- Diagnostics ZIP SHA-256: `09698c323da3a5eb5afff04e8742de7dabe79b84af6c9aec7e0e2552928ff533`.

## Checks performed for 1.3.2

- 6/6 native arm64 CTest passed (9.87 s); six x86_64 test binaries passed under Rosetta.
- Both final signed-app backend smoke modes exited 0.
- Candidate GUI: restored window, minimum 900×560 and resized 1043×638 window, native zoom, filter/no matches/clear, host selection, immediate Ping/Trace availability, successful self-IP Ping and tab synchronization; real /24 scan 254/254.
- Final stable GUI: native startup and actual /24 discovery; RU/EN screenshots captured directly from the final app, no sample rows. Exact results recorded in build/validation/macos-native/result-1.3.2.json.
- Profile/report/monitor runtime checks are part of the six suites. Broader Cocoa profile/export and 120-round resource checks were performed for 1.3.1 and are historical evidence, not repeated 1.3.2 GUI checks.

## Delivered UI corrections

Restored/resized window dimensions drive the layout. The table and detail pane fill the available width; the excess top gap is removed. Compact labeled native toolbar actions, a leading detail segmented control, a quiet sidebar and useful empty states replace the broken layout. Selecting a host immediately enables eligible diagnostics. Detail text wraps without horizontal scrolling. Candidate packaging metadata is kept separate from stable metadata.

## Limits

Intel hardware, older macOS, physical NIC unplug/display scaling, controlled ARP fixtures, full external-client and multi-hop Trace checks, and hours/days of GUI monitoring were not tested. Rosetta is not native Intel validation. macOS 13 is a deployment baseline, not a runtime test. No Gatekeeper/notarization success is claimed. Optional mDNS, active IPv6 and persistent disk history remain future scope.

Evidence: build/validation/macos-native. Current raw diagnostics: build/unpacked/1.3.2/macos-native-universal. User settings/profiles are retained; obsolete local packages and temporary verification files are cleaned after publication. GitHub release history is retained.
