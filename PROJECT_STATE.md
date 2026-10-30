# Look@LAN-ng 1.4.0 — validated Windows/Linux candidate

30 October 2026. Windows x86/x64/ARM64 and Linux x64 built locally on Windows. macOS builds/releases independently on Mac; no Mac compilation performed here.

Implemented profile v2 + bounded binary host/event snapshots, temporary mode without INI creation, singleton profile autoload, Windows System/Light/Dark themes with persistent preference only after Save Profile, and seamless native menu/toolbar painting.

Windows x86/x64 17/17 tests each and packed smokes passed. Native ARM64: 13 suites + four packed smokes, run 41058601241. Windows real GUI profile/theme journey: 25 checks. Ubuntu: nine native suites, 39 GTK workflows, packed startup/LAN scan, 100 Stop cycles FD 6→6, max11ms.

Latest verified tools: stable Clang 23.1.4 / LLVM-MinGW 20261020; IEEE MA-L40421; stable UPX5.3.0 maximum/integrity. See [validation](docs/VALIDATION_1.4.0.md), [notes](docs/RELEASE_NOTES_1.4.0.md), [profiles](docs/PROFILES.md). Remaining hardware/OS matrix is explicitly unverified. Publication pending; older GitHub releases may only be removed after new downloads verify.

Earlier records: [project history](docs/PROJECT_HISTORY.md). Permanent workstation/build/persistence rules: AGENTS.md.
