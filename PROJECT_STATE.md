# Current project state — 13 October 2026

## macOS 1.3.1

Stable universal arm64+x86_64 was built locally on Mac and published as the current GitHub release: https://github.com/nr084/Look-LAN-ng/releases/tag/v1.3.1 . GitHub latest/draft=false/prerelease=false verified. Both assets were downloaded back and matched SHA-256; both screenshot PNGs match the tagged sources. This workspace builds only macOS. Windows and Linux were not compiled or tested here; their public releases remain Windows 1.2.0 and Linux 1.3.0.

Checks passed: 6/6 arm64 CTest, 6/6 Rosetta x86_64 suites, both signed-app smoke modes, Cocoa GUI/actual LAN discovery, all report formats and Unicode profiles. Resource checks: 100 Scan/Stop FD 3→3, 120 scheduled monitoring rounds (~10 minutes), FD 3, RSS baseline 11664/max 11696 KiB. Native signing, DMG integrity/mount/hash/modes passed. Real RU/EN final-build screenshots taken on this Mac.

Delivered fixes: local-endpoint interface binding, ordinary ICMP no-route outcomes no longer failing a round, translated range placeholder, immediate General refresh after saving a device note. Native Mac build/resource/packaging and checked GitHub publication scripts are in tools.

Evidence, exact hashes and remaining Intel/older macOS/physical NIC/DPI/long-runtime limits: [MACOS_VALIDATION.md](docs/MACOS_VALIDATION.md). The bundle is ad-hoc signed, not Developer-ID notarized. Rosetta does not substitute for Intel hardware. Optional mDNS/active IPv6/persistent disk history remain future scope.

Sources are committed through the GitHub checkout prepared from current main; local release assets and raw diagnostics remain in dist and build/unpacked. Temporary export/profile/packaging and download-back verification files are cleaned. Settings and user profiles are retained. Release source tag points to e573a1d8abcdca076f9e793284b5d629ecfa6ef0. Publication receipt: build/validation/macos-native/github-release-1.3.1.json.

## Existing releases

- Windows 1.2.0: three architecture ZIPs, historical test evidence in docs/RELEASE_CHECK.md.
- Linux 1.3.0: one x64 ZIP, historical native results in docs/LINUX_VALIDATION.md.
- Their assets/history are preserved and are not re-certified by this Mac release.

Previous milestone/candidate records are archived in [PROJECT_HISTORY.md](docs/PROJECT_HISTORY.md).
