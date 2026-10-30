# macOS 1.4.0 — native validation, 30 October 2026

Built directly on Apple Silicon, macOS 27.2; universal **arm64+x86_64**, deployment baseline **macOS 13**. Windows/Linux were not compiled on this Mac. Their four published v1.4.0 ZIPs and the existing tag are preserved.

## Compiler and MAC database

Every `tools/build-macos.sh` invocation runs fail-closed `build_preflight.py macos`. It checks official LLVM latest stable, updates/uses Homebrew LLVM, downloads official IEEE MA-L and records date/SHA-256. Downloads have a bounded total timeout and TLS verification. A failed freshness check stops compilation. CMake uses `--fresh` to prevent compiler changes from resetting universal architecture options; final lipo checks require both slices.

- Compiler: **Clang 23.1.4**; [official stable release](https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.4).
- Checked UTC: `2026-10-30T13:58:03.250701+00:00`. Compiler SHA-256: `a3f510aa37c9a3082649435e39e3f7c808b6696bb62f077b141c271b5aa783a2`.
- IEEE: 40421 MA-L prefixes; source https://standards-oui.ieee.org/oui/oui.csv .
- Retrieved UTC: `2026-10-30T13:58:03.248058+00:00`.
- Source CSV SHA-256: `6ef6e160740fda9ba94be57484d7a333eb091543c809437b443db1915d2fed93`.
- Embedded OUI binary SHA-256: `1bba0ae02f0e2d2e783c9a253a7ffab8f795fe1f2c6d91533d12c3499b432a5c`.

## Automated final-build checks

- **9/9 ARM64 CTest passed** (13.05 s): history roundtrip/CRC/bounds, atomic history store, profile sessions, native POSIX runtime, core, features, monitoring, profile/report and NBNS/details.
- **9/9 x86_64 binaries passed under Rosetta**. This is not an Intel hardware test.
- Both signed-app backend smoke modes exited 0.
- Profile session tests cover no settings before Save, singleton selection and history continuity, theme readback and refusal to overwrite invalid existing INI.
- POSIX tests cover real local networking, exports, profiles, diagnostics and 100 Scan/Stop cycles. These are backend checks, distinct from Cocoa checks below.

## Final Cocoa GUI checks

The final signed app was launched with an isolated validation data path, leaving existing user settings/profiles unchanged (SHA-256 checked).

- Real en0 /24 scan: **254/254, 8 online, 18.953 s**; live progressive rows, selection and details.
- RU and EN; default System appearance, Light and explicit Dark. Language/theme changes in temporary mode created no INI or data directory.
- Save Profile with a Unicode filename created a **v2** `.lanprofile`, `.lanprofile.lanhistory` and INI in the managed `profiles` data area. Successful save enabled persistence.
- Native monitoring performed a complete repeat and a further round; Stop cancelled work safely. First-seen times remained unchanged while last-seen times advanced.
- Native SavePanel CSV/TXT/HTML exports with Unicode filenames read back successfully; CSV was exported during monitoring.
- Clean quit/relaunch automatically selected the sole saved profile. Eight rows returned as HISTORY (not fabricated current reachability); first/last-seen data, scan/monitor/export log and English/Light preference restored.
- Opening a legacy v1 profile reported failure; its bytes and the current v2 profile were unchanged. No migration occurred.
- Invalid INI preservation is additionally covered by automated profile-session tests.

## Package and screenshots

Ad-hoc hardened-runtime signature, deep/strict verification, actual DMG mount, hashes and file modes passed. Both slices link only to macOS system libraries/frameworks; no Homebrew runtime dependency. UPX latest stable 5.3.0 has no supported macOS packer; universal Mach-O and signing remain intact. DMG uses HFS+ ULMO/LZMA. Diagnostic ZIP/raw builds remain **local only**.

- DMG: `look@lan-ng-macos-universal-1.4.0.dmg`, 739782 bytes.
- SHA-256: `f3172321a372a702e6cd45a9f2d112bd449bbaf85ab233e26fe0f2989865c1d7`.
- Signed Mach-O SHA-256: `9c9deb1eed3dfa66e8166eb58f5a104723a71b90753f4e7c342a1efd33b2cdb4`.

Final RU/Dark and EN/Light screenshots follow the real scan. All address-bearing regions (interface/range/network info, sidebar, table IP/host/MAC and details) are strongly blurred using deterministic downsampling plus Gaussian blur, then visually checked for unreadability. Only redacted outputs are published. Existing current Windows/Linux screenshots were also reviewed; current issues/comments contained no images. Old Mac screenshot files are absent from the current tree.

## Limits

Native Intel hardware and older macOS were unavailable; macOS 13 is a deployment target, not a tested OS. No Developer ID/notarization or quarantine/Gatekeeper success is claimed. Physical NIC unplug/display scaling, actual high-contrast switching, all external client actions and multi-hop external Trace, and hours/days GUI monitoring remain untested. Cocoa uses native system appearance and high-contrast precedence. Optional mDNS, active IPv6 and persistent per-device notes/Ping/Trace samples remain out of scope; profile/host/event history persistence is implemented and checked.

Evidence: `build/validation/`; current diagnostic raw build: `build/unpacked/1.4.0/macos-native-universal/`. Temporary GUI fixtures/raw screenshots and old local deliveries are removed after publication. Original user profiles/settings are retained.

## Mac publication verified — 30 October 2026

Published DMG in existing stable [v1.4.0](https://github.com/nr084/Look-LAN-ng/releases/tag/v1.4.0). All five assets downloaded back and SHA-256 matched; four Windows/Linux asset IDs/sizes/digests/URLs and notes are unchanged. Existing tag: `bad061a71231af9701595224017f729162035275`; Mac source: `07a31abf673c16806d8c568bd14a54084f5f6ac3`. Redacted screenshots verified at the source commit. No public Mac diagnostics ZIP. Old local Mac 1.3.2 deliveries/raw and obsolete Windows 1.3.2 caches removed; temporary GUI exports/profiles and unredacted captures deleted. Original user files unchanged. Current release/raw diagnostics/build tools retained. Receipts: build/validation/github-macos-1.4.0-published.json and cleanup-macos-1.4.0.json.
