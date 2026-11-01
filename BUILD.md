# Build / Сборка

На этом Windows рабочем месте собираются и публикуются только Windows x86/x64/ARM64 и Linux x64. Mac собирает, проверяет и публикует свою universal DMG отдельно. Сборка в GitHub Actions запрещена; существующий ARM64 workflow запускает только локально собранные бинарные файлы.

This Windows workspace builds and publishes Windows x86/x64/ARM64 and Linux x64 only. macOS universal DMG is built, tested and published independently on Mac. GitHub Actions runs prebuilt ARM64 validation binaries only.

## Windows host

Python 3, PowerShell 7, CMake/Ninja and the existing Ubuntu 22.04 cross sysroot/GTK metadata are build-time tools, not application runtime dependencies.

```powershell
python tools/build-current.py --arch x86
python tools/build-current.py --arch x64
python tools/build-current.py --arch arm64
python tools/build-current.py --target linux
```

Each invocation checks official LLVM and LLVM-MinGW stable releases, installs the verified latest toolchain into `build/toolchains`, and downloads current IEEE MA-L data over validated HTTPS. Preview/RC versions, missing upstream SHA-256 or failed freshness checks stop the build. A cached compiler is reused only when its release provenance and executable hash match. Reports record the actual compiler, source URL, retrieval time and database hashes in `build/validation/build-preflight-*.json`; each target also retains its own `preflight.json`.

`tools/build-windows.ps1`, `tools/build-windows-arm64.py` and `tools/build.cmd` delegate to the same mandatory preflight. Direct manual `cmake --build` is a developer operation and must be preceded by that preflight; release builds use the entry points above. Old MSVC/cross-Mac commands are retired.

The Linux compiler runs locally on Windows; the Ubuntu VM runs native/GUI tests only. x86/x64 run CTest locally. ARM64 binaries are validated on a native ARM64 runner without remote compilation. Run the exact packed files through smoke/GUI and profile-session tests before publication.

```powershell
./tools/package-windows-arch.ps1 -Architecture x86
./tools/package-windows-arch.ps1 -Architecture x64
./tools/package-windows-arch.ps1 -Architecture arm64 -DeferNativeArm64
python tools/package-ports-windows.py --target linux
./tools/collect-windows-release.ps1
```

ARM64 deferral is a local packaging step, not permission to publish untested files. Native validation must match the final EXE hash. Stable UPX is checked again at packaging time; maximum flags, integrity and launch tests are required. Retain current raw binaries and reports locally. Never publish macOS diagnostics ZIPs or user INI/profiles/history.

## Publication

Use `tools/publish-local-release.ps1` with the verified Windows ZIPs and Linux ZIP, matching native proof and RU/EN notes. Verify downloads, then remove earlier GitHub releases per the user's standing instruction; source Git history is not rewritten. A later Mac publication adds its verified DMG to the current release while preserving Windows/Linux artifacts and notes.

Capture final real Windows/Ubuntu scans; blur every IP before committing screenshots. Remove obsolete screenshot files and temporary captures. Clean old local delivery packages only after the new packages and necessary diagnostics exist; never delete user settings/profiles/history.

## Native macOS workspace

On Mac, use `bash tools/build-macos.sh`: it checks official latest stable LLVM/Homebrew, downloads fresh IEEE MA-L, records versions/date/hashes, configures a fresh universal CMake cache and checks arm64+x86_64 slices. No Windows/Linux compilation is done on Mac. Run native CTest and Rosetta tests, validate Cocoa with an isolated `--validation-data-dir /absolute/test/path`, then package with `python3 tools/package-macos.py --release`. Keep the raw diagnostics local.

Publish only the checked DMG using `tools/publish-macos-release.py --version 1.4.0 --source-repo <clean-checkout> --notes docs/RELEASE_NOTES_MACOS_1.4.0.md --add-assets-to-published-release`. This preserves the four Windows/Linux assets, their IDs/sizes/hashes and the existing tag; it replaces only the marked macOS notes block. Every screenshot must have all IP/MAC regions strongly blurred and checked before upload.

## Repository contents / Содержимое репозитория

GitHub stores source code, build/test tools, release packages and public project/user documentation. Agent instructions, working notes, status journals and internal validation reports stay local and are excluded by `.gitignore`. Public release notes retain test summaries and known limitations.

На GitHub хранятся исходники, файлы сборки/тестов, релизы и публичные описания. Агентские инструкции, рабочие заметки, журналы состояния и внутренние отчёты остаются локально. Результаты проверок и известные ограничения кратко указываются в описании релиза.
