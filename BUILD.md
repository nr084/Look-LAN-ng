# Сборка и проверки

Требования: Windows x64, MSVC v143 из Visual Studio Build Tools 2022, Windows 10/11 SDK, CMake и Ninja. Установщик сборочных инструментов нужен только разработчику, не пользователю EXE.

## Готовый сценарий

Из корня проекта в PowerShell:

```powershell
cmd /c tools\build.cmd
```

Сценарий находит Visual Studio через vswhere, устанавливает x64 environment, конфигурирует Release, собирает и запускает CTest. Ошибки возвращаются через exit code.

Результат: `build/Look@LAN-ng.exe`. Для portable-поставки достаточно этого файла.

## Вручную

В **x64 Native Tools Command Prompt for VS 2022**:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Если cmake/ninja не найдены, включите компонент CMake tools в Visual Studio Installer либо добавьте их каталоги в PATH.

Для пути с кириллицей используйте перед конфигурацией `chcp 65001` и добавьте `-DCMAKE_TRY_COMPILE_CONFIGURATION=Release` к команде cmake. Готовый сценарий уже включает эти настройки: они устраняют обнаруженную на этой машине ошибку C1902 при initial Debug try-compile.

Release: C17, Unicode W API, /MT, /O2, /GL, /LTCG, /NXCOMPAT, /DYNAMICBASE, /HIGHENTROPYVA. Начиная с v0.3 /guard:cf убран по прямому указанию пользователя для совместимости с UPX. Связывание только с системными Windows DLL.

## Smoke test окна

```powershell
$smoke = Start-Process -FilePath '.\build\Look@LAN-ng.exe' -ArgumentList '--smoke-test' -PassThru -WindowStyle Hidden
$smoke.WaitForExit()
$smoke.ExitCode
```

Приложение создаёт native controls, перечисляет интерфейсы и закрывается по timer. Активное сетевое сканирование не выполняется, настройки не сохраняются. Это проверка запуска, а не полноценная проверка визуального оформления.

CTest также запускает backend integration test: пробы только собственного IPv4/localhost и созданного тестом TCP listener, 500 warm-up циклов и затем 500 повторных/отменённых scans с контролем HANDLE count. При отсутствии IPv4-интерфейса этот тест помечается skipped. Smoke test окна проверяет русский/английский текст, встроенную OUI-базу, фильтр и detail tabs. Feature tests проверяют TCP refused/open distinction, port parser и vendor table validation.

В v0.3 добавлены portable monitor tests: NEW/baseline, ONLINE→OFFLINE→ONLINE, два пропуска, отмена, stale DNS/cache, MAC/name/IP changes, scoped rescan и bounded log. Backend test также проверяет discovery-only monitoring round на собственном IP. UI smoke проверяет History/Log и включение/отключение monitoring без активного сетевого сканирования.

Запускайте TCP integration из обычной консоли с доступом к локальным sockets. Ограниченный sandbox может возвращать WSAEACCES 10013 даже для тестового listener; это не требует Administrator для обычного запуска программы. Для отдельной диагностики listener: `build\backend_tests.exe --tcp-only`. Для сборки без запуска тестов: `cmd /c tools\build.cmd --build-only`.

В v0.4 CTest включает семь проверок: core, features, monitor, profiles/reports, backend, UI и Windows files. Проверяются объединение диапазонов, UTF-8 round-trip, отказ без изменения профиля, экранирование отчётов, статистика, применение настроек к backend и отсутствие вымышленного MAC. Files tests проверяют Unicode-пути, снимок фонового экспорта и сохранение назначения при отмене, затем удаляют свои файлы. UI smoke дополнительно проверяет RU/EN редактор профиля и отрисовку GDI-статистики. Standard file dialogs используют системную COMDLG32, дополнительных runtime нет.

В v0.4.1 добавлен восьмой тест `resolver_tests`: реальный resolver компилируется с deterministic API doubles. Без DNS-трафика проверяются synchronous/asynchronous success, NXDOMAIN, pending cancellation, timeout, сохранение выходных значений и освобождение records/events. Backend integration дополнительно проверяет реальный localhost, 20 отмен hostname scan без NET_ERROR, освобождение всех workers и 30 init/shutdown cycles. Системная DNSAPI добавлена к Windows imports.

В v0.4.2 добавлены `ping_tests` и `ui_close_scan` — всего 10 CTest. Ping tests компилируют реальный pool с deterministic ICMP doubles: sync/async/error, освобождение worker до 5000 ms timeout, lifetime payload/reply, предел 256 requests, заполненный pool, delayed shutdown и 320 retire/reap cycles без роста handles. Это не проверка реальной неотвечающей LAN.

`ui_close_scan` запускает скрытое окно с `--smoke-close-scan`: обычные UI проверки, затем discovery собственного IPv4 и немедленный WM_CLOSE. Проверяется освобождение backend перед завершением процесса. При отсутствии IPv4 тест skipped. INI не записывается. Этот режим использует сеть только на собственный IP; обычный `--smoke-test` остаётся без scan. Backend lifecycle тест теперь включает 15 завершений после запуска coordinator из 30 cycles.

В stable 1.0.0 всего 12 CTest. `neighbor_tests` проверяет реальный ARP pool с API double, который остаётся заблокирован после cancel: source/interface, invalid MAC, сохранение output, отсутствие преждевременного освобождения и delayed shutdown. `ui_dpi` проверяет 100 synthetic DPI transitions (100/125/150/175/200%, RU/EN, два размера окна), filter overlap и bounded GDI/USER count. Длинный hostname 253 bytes проверяется в native ListView; core включает /23 boundary.

Backend прогревается 500 scan/cancel cycles, затем выполняет ещё 500 с выборками каждые 100 и сравнивает HANDLE count после quiescence DNS/ICMP/coordinator. Удлинённый warm-up покрывает наблюдавшийся bounded +2 рост Windows-managed resources в первом коротком прогоне; дальнейший рост не допускается. Timeout backend CTest — 60 s. Это local self-host test, не длительный LAN/Windows matrix. Лимит tempo применяется к initial и monitoring probes.

Оригинальные ресурсы иконки пересоздаются командой `.\tools\generate-icons.ps1` (System.Drawing, build-time only). `--smoke-preview` создаёт `build/ui-preview.bmp` с явно помеченными sample data, без scan/INI; off-screen окно используется только для native capture и не активируется. Обычная поставка не создаёт preview.

## Windows x64 ZIP

В 1.1 зарегистрировано 15 CTest: добавлены `details_nbns_tests`, `tools_tests`, `ui_close_tools`. Проверяются scoped notes/capacity, ping-ring eviction, NBSTAT wildcard/transaction/length/group/name validation, Trace TTL progression/destination stop/30-hop cap и cancel, реальные четыре own-IP Ping и one-hop Trace. UI smoke проверяет отображение notes/Ping/Trace и закрытие с активной диагностикой. SSH не запускается автоматически тестами; проверка клиента выполняется только по явному UI-действию.

После упаковки дополнительно выполняется packed `--smoke-close-tools`. UI tests и API doubles не заменяют real Windows/NIC/LAN matrix.

Готовый EXE хранится в `dist/<версия>/Look@LAN-ng.exe`; ZIP содержит его с обычным именем. Flat `dist/Look@LAN-ng.exe` обновляется по возможности. Если старая версия запущена и удерживает файл, versioned EXE/ZIP создаются независимо; JSON сообщает `exe_path` и `latest_exe_updated=false`. Пользовательский процесс не завершается скриптом.

После успешных сборки и тестов:

```powershell
.\tools\package-win64.ps1
```

Сценарий сверяет latest stable UPX с официальным GitHub API, скачивает win64 asset, проверяет SHA-256 digest при наличии upstream digest, сжимает с `--ultra-brute --lzma`, проверяет `upx -t` и запуск final EXE. Требуется сеть только разработчику во время упаковки.

Перед записью ZIP packed EXE проходит startup, DPI и close-scan. Metadata сохраняются в `dist/release-info.json` и versioned JSON. ProductVersion задаёт stable/rc имя поставки; FileVersion остаётся числовым. Нет внешней публикации: скрипт создаёт локальные artifacts.

Ошибки упаковки/проверки останавливают публикацию нового ZIP. GUARD_CF уже удалён из параметров сборки v0.3 по указанию пользователя. Несжатая копия остаётся в `build/unpacked`; UPX download/extraction stage удаляется в finally. ZIP получает имя `Look@LAN-ng-<версия>-win64.zip`. «Публикация» здесь означает только запись локального release artifact в `dist`, без загрузки во внешние сервисы.

## Core без Windows

На системе с C17 compiler:

```sh
cmake -S . -B build-core
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

Windows GUI target создаётся только при WIN32. Межплатформенная сборка core пока не проверена на Linux/macOS.

## Файлы сборки

`build` содержит готовый EXE и воспроизводимые результаты CMake/Ninja. Для очистки закройте EXE и удалите только каталог `build` внутри проекта. Исходные пользовательские attachments не изменяются и не удаляются.

## Windows x86 / x64 / ARM64 and GitHub releases

`tools/build-windows.ps1 -Architecture x86|x64|arm64` uses separate build/windows-<arch> directories and Visual Studio 2022 multi-config Release builds; CTest is invoked with -C Release. Install the corresponding MSVC target tools. Local x86 runs under WOW64; ARM64 runs natively on the GitHub windows-11-arm runner. ASLR/NX are enabled for all architectures, HIGHENTROPYVA only for 64-bit.

`tools/package-windows-arch.ps1` verifies the official latest stable UPX archive digest, compresses with --ultra-brute --lzma, checks integrity and executes all four packed smoke modes on the corresponding runner. Architecture build-info.json is retained as a CI artifact.

`tools/collect-windows-release.ps1` validates executable hashes and consistent versions, then creates three ZIPs, each containing only Look@LAN-ng.exe: look@lan-ng-x86-<version>.zip, look@lan-ng-x64-<version>.zip, look@lan-ng-arm64-<version>.zip. SHA-256 ZIP hashes are appended to the bilingual release notes.

`.github/workflows/release.yml`: a v<version> tag builds/tests/packages all three architectures and publishes the release after all jobs succeed. workflow_dispatch builds the same artifacts without publishing. Update CMake version, app.manifest and docs/RELEASE_NOTES.md before tagging. Releases go to https://github.com/nr084/Look-LAN-ng/releases .
