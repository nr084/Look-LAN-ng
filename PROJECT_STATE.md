# Look@LAN-ng 1.4.0 — published Windows/Linux release

30 October 2026. Windows x86/x64/ARM64 and Linux x64 built locally on Windows. macOS builds/releases independently on Mac; no Mac compilation performed here.

Implemented profile v2 + bounded binary host/event snapshots, temporary mode without INI creation, singleton profile autoload, Windows System/Light/Dark themes with persistent preference only after Save Profile, and seamless native menu/toolbar painting.

Windows x86/x64 17/17 tests each and packed smokes passed. Native ARM64: 13 suites + four packed smokes, run 41058601241. Windows real GUI profile/theme journey: 25 checks. Ubuntu: nine native suites, 39 GTK workflows, packed startup/LAN scan, 100 Stop cycles FD 6→6, max11ms.

Latest verified tools: stable Clang 23.1.4 / LLVM-MinGW 20261020; IEEE MA-L40421; stable UPX5.3.0 maximum/integrity. See [validation](docs/VALIDATION_1.4.0.md), [notes](docs/RELEASE_NOTES_1.4.0.md), [profiles](docs/PROFILES.md). Remaining hardware/OS matrix is explicitly unverified. Published: https://github.com/nr084/Look-LAN-ng/releases/tag/v1.4.0 . All four public ZIP downloads match local SHA-256. Older releases v1.2.0, v1.3.0, v1.3.1 and v1.3.2 were removed after verification. Current GitHub tree/release pages contain only the two IP/MAC-blurred 1.4.0 screenshots; issues/comments contain no other images. Source Git history/tags were not rewritten.

Earlier records: [project history](docs/PROJECT_HISTORY.md). Permanent workstation/build/persistence rules: AGENTS.md.

## macOS 1.4.0 — native Mac addition, 30 October 2026

Universal signed app/DMG built on Mac with official stable LLVM 23.1.4 and fresh IEEE MA-L 40421. Nine native and nine Rosetta suites plus both final signed-app smokes passed. Real LAN: 254/254, 8 online, 18.953 s. Cocoa profile v2, temporary/persistent mode, history restart, theme preferences, RU/EN, monitoring and three Unicode report formats checked. All original user-data hashes unchanged. Legacy profile rejection preserved file bytes. Mac screenshot address regions strongly blurred and visually reviewed. DMG is added to the existing v1.4.0; four Windows/Linux ZIPs and tag preserved, diagnostics stay local. Native Intel/older macOS/notarization remain unverified. See docs/MACOS_VALIDATION.md and docs/RELEASE_NOTES_MACOS_1.4.0.md.

## Mac publication verified — 30 October 2026

Published DMG in existing stable [v1.4.0](https://github.com/nr084/Look-LAN-ng/releases/tag/v1.4.0). All five assets downloaded back and SHA-256 matched; four Windows/Linux asset IDs/sizes/digests/URLs and notes are unchanged. Existing tag: `bad061a71231af9701595224017f729162035275`; Mac source: `07a31abf673c16806d8c568bd14a54084f5f6ac3`. Redacted screenshots verified at the source commit. No public Mac diagnostics ZIP. Old local Mac 1.3.2 deliveries/raw and obsolete Windows 1.3.2 caches removed; temporary GUI exports/profiles and unredacted captures deleted. Original user files unchanged. Current release/raw diagnostics/build tools retained. Receipts: build/validation/github-macos-1.4.0-published.json and cleanup-macos-1.4.0.json.
