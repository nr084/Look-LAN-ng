# Current project state — 14 October 2026

## macOS 1.3.2

Published as latest stable on GitHub: https://github.com/nr084/Look-LAN-ng/releases/tag/v1.3.2 . Built and checked locally on Mac, universal arm64+x86_64. This workspace builds only macOS. Windows 1.2.0 and Linux 1.3.0 stay in GitHub history.

Cocoa layout fills the restored/resized window, removes the excess top gap, and uses labeled native toolbar buttons, compact detail tabs and useful empty states. Host selection immediately validates Ping/Trace; detail text wraps. Native arm64 CTest 6/6 (9.87 s), six Rosetta x86_64 suites and both final signed-app backend smoke modes passed. Final stable LAN scan: 254/254, eight online hosts, 18.974 s. Final RU/EN screenshots are in docs/images/macos-1.3.2-*.png. Candidate resize/filter/Ping checks are documented separately from final stable checks.

Ad-hoc signature and ULMO/LZMA DMG mount/hash/modes verified. UPX latest stable 5.2.1 disables macOS packing. Intel hardware, older macOS, full NIC/DPI/long-runtime and notarization remain unverified/not done. [Validation](docs/MACOS_VALIDATION.md), [release notes RU/EN](docs/RELEASE_NOTES_1.3.2.md).

Obsolete local Mac 1.3.1 and 1.3.2-rc.1 deliveries/raw builds and temporary package/download verification files have been removed. The temporary source checkout is deleted after this documentation push. Current platform deliveries, 1.3.2 raw diagnostics/build tools, source/docs and user settings/profiles are retained. GitHub release history is retained. Prior state is in [PROJECT_HISTORY.md](docs/PROJECT_HISTORY.md).

Publication verified: latest/draft=false/prerelease=false; both assets downloaded back and SHA-256 matched; both screenshots match the release tag. Release source commit: 5e862108e433e3dc040e2af73b77bbb251a9cba7. Receipt: build/validation/macos-native/github-release-1.3.2.json.
