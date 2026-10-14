# Current project state — 14 October 2026

Windows x86/x64/ARM64 and Linux x64 are updated to 1.3.2, compiled locally on
Windows from the current Mac-release source. No Mac compilation or repackaging
was performed here. Existing macOS universal 1.3.2/tag/assets are preserved.

Windows x86/x64: 15/15 CTest each (54.13/53.46 s) and all four final packed smokes.
ARM64: eleven local C suites + four packed smokes on native Windows 11 ARM64,
runtime-only https://github.com/nr084/Look-LAN-ng/actions/runs/38566307932 . All
compilation is local. UPX 5.2.1 maximum/integrity passed for all Windows and Linux.

Ubuntu 26.04.1 x64 VM: six native suites, packed startup, 39/39 GTK workflows;
100 Scan/Stop cycles FD 6→6, maximum Stop 12 ms. Actual final screenshots:
docs/images/windows-1.3.2.png and linux-1.3.2.png. README/release notes RU/EN
are refactored around all platforms and actual validation boundaries.
Reports: docs/WINDOWS_LINUX_VALIDATION_1.3.2.md; dist/*-release-validation.json.

The existing release is extended add-only with three Windows ZIPs and one Linux
ZIP; existing Mac assets/tag are not replaced. Full hardware/older OS/NIC/DPI/
long-runtime matrices remain unverified. Current raw builds and reports retained;
owned VM temp data, private CI staging/secret and obsolete local outputs cleaned
after fetching evidence. Historical Mac/source state follows below.

---

# Current project state — 14 October 2026

## macOS 1.3.2

Published as latest stable on GitHub: https://github.com/nr084/Look-LAN-ng/releases/tag/v1.3.2 . Built and checked locally on Mac, universal arm64+x86_64. This workspace builds only macOS. Windows 1.2.0 and Linux 1.3.0 stay in GitHub history.

Cocoa layout fills the restored/resized window, removes the excess top gap, and uses labeled native toolbar buttons, compact detail tabs and useful empty states. Host selection immediately validates Ping/Trace; detail text wraps. Native arm64 CTest 6/6 (9.87 s), six Rosetta x86_64 suites and both final signed-app backend smoke modes passed. Final stable LAN scan: 254/254, eight online hosts, 18.974 s. Final RU/EN screenshots are in docs/images/macos-1.3.2-*.png. Candidate resize/filter/Ping checks are documented separately from final stable checks.

Ad-hoc signature and ULMO/LZMA DMG mount/hash/modes verified. UPX latest stable 5.2.1 disables macOS packing. Intel hardware, older macOS, full NIC/DPI/long-runtime and notarization remain unverified/not done. [Validation](docs/MACOS_VALIDATION.md), [release notes RU/EN](docs/RELEASE_NOTES_1.3.2.md).

Obsolete local Mac 1.3.1 and 1.3.2-rc.1 deliveries/raw builds and temporary package/download verification files have been removed. The temporary source checkout is deleted after this documentation push. Current platform deliveries, 1.3.2 raw diagnostics/build tools, source/docs and user settings/profiles are retained. GitHub release history is retained. Prior state is in [PROJECT_HISTORY.md](docs/PROJECT_HISTORY.md).

Publication verified: latest/draft=false/prerelease=false; both assets downloaded back and SHA-256 matched; both screenshots match the release tag. Release source commit: 5e862108e433e3dc040e2af73b77bbb251a9cba7. Receipt: build/validation/macos-native/github-release-1.3.2.json.

Additional workspace cleanup: removed remaining Windows/Linux 1.3.1-rc.1 build caches and raw candidate binaries, obsolete Mac candidate screenshots/package logs and empty cross-build directories (115722677 bytes). Current Windows 1.2.0/Linux 1.3.0/macOS 1.3.2 delivery packages and published raw diagnostics retained; protected hashes unchanged. Receipt: build/validation/workspace-cleanup-2026-10-14.json.
