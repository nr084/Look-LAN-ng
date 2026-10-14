# Windows and Ubuntu/Linux 1.3.2 — 14 October 2026

Both platforms were compiled locally on Windows from the current 1.3.2 source.
macOS was neither compiled nor repackaged here; its existing published universal
DMG, diagnostics and tag are preserved. This work resumes checked publication
for Windows/Linux; the earlier no-test exception applied to the private rc.1 stage.

## Windows

| Target | Local compiler | Runtime checks |
| --- | --- | --- |
| x86 | MSVC v143 | 15/15 CTest on Windows 11 x64, 54.13 s; four final packed smoke modes |
| x64 | MSVC v143 | 15/15 CTest on Windows 11 x64, 53.46 s; four final packed smoke modes; actual LAN screenshot |
| ARM64 | LLVM-MinGW 23.1.3 | 11 C suites and four final packed smoke modes on native Windows 11 ARM64, build 26200 |

ARM64 [runtime-only job](https://github.com/nr084/Look-LAN-ng/actions/runs/38566307932)
downloads locally compiled binaries; it never compiles. Its account-token
permission remains `contents: read`. An ephemeral encrypted, file-scoped download
URL permits reading only the private test archive; it is removed before execution
and the secret/private staging draft is removed after validation. The initial
read-only API download was rejected by GitHub before any binary executed; that
failed run is not counted as a successful check.

Packed startup, simulated DPI, close during scan and close during diagnostics
passed for all three architectures. Latest official stable UPX 5.2.1 was checked;
archive digest and `upx -t` passed with `--ultra-brute --lzma`. Raw builds and
architecture `build-info.json` remain local. Each delivered ZIP contains one EXE.

Actual Windows screenshot: `docs/images/windows-1.3.2.png`, captured from the
final packed x64 executable after scanning 254 addresses; ten online hosts plus
an unconfirmed cached record. Unknown cache status is not reported as online.
Only this application's window was captured; its process was closed afterwards.

Full Windows 10/11 hardware, physical DPI moves, all NIC/failure combinations and
long-duration resource checks were not repeated/are not claimed complete.

## Ubuntu/Linux

Build baseline: Ubuntu 22.04 GTK3/glibc sysroot, x64, locally cross-compiled on
Windows. Runtime environment: user's Ubuntu 26.04.1 LTS x64 VM, ordinary user.

- Six native suites passed: core, features, monitor, profiles/reports,
  details/NBNS and POSIX runtime. Packed own-interface startup passed.
- POSIX runtime covers scoped source/name cache, MAC invalidation, stale request
  generations, missing interfaces/remapping, DNS parsing and a source-bound UDP
  fixture, monitoring, history/notes, Ping/Trace, cancellation, immutable async
  exports, Unicode profiles and cancelled export preserving the destination.
- 100 Scan/Stop cycles: FD 6 → 6; maximum Stop 12 ms; idempotent close passed.
- 39/39 GTK workflows passed against the final packed executable, including
  profiles, exports, monitoring, diagnostics, language persistence and close.
- Separate actual default-subnet screenshot: 254 addresses, eleven online
  devices, about 19.2 s, selected local host; no sample data. Capture used an
  isolated Xvfb/Openbox GUI session on Ubuntu. `docs/images/linux-1.3.2.png`.

The current run does not repeat the historical 1.3.0 ARP-only/stale-cache
fixtures or 120-round monitoring stress; those are described as historical in
the shipped README. Ubuntu 22.04 native startup, physical HiDPI/NIC unplug,
OEM non-ASCII NBNS and hours/days of load remain unverified. No VM compilation.

## Evidence and cleanup

Local evidence: architecture CTest logs under `build/windows-*/Testing`,
`build/linux-1.3.2-native.log`, `build/linux-1.3.2-gui.log`, GUI JSON and native
ARM64 logs under `build/validation/arm64-native`. Release SHA-256 values appear
in the bilingual release notes. Package hashes are bound to the native proofs.
Owned VM test/temp files and obsolete local deliveries are removed after reports
are fetched; current deliveries, source, diagnostics and user settings remain.
