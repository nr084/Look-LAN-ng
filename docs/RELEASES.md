# Правила будущих релизов

Зафиксировано по указаниям пользователя 12 октября 2026 года.

| Платформа | Имя поставки |
| --- | --- |
| Windows 32-bit | `Look@LAN-ng-<версия>-win32.zip` |
| Windows 64-bit | `Look@LAN-ng-<версия>-win64.zip` |
| Linux | `Look@LAN-ng-<версия>-linux.zip` |
| macOS | `Look@LAN-ng-<версия>-macos.dmg` |

Пример: `Look@LAN-ng-0.2.0-win64.zip`. В Windows ZIP приложение находится как `Look@LAN-ng.exe`; сохранение настроек остаётся portable. Архитектуры Linux/macOS уточняются при реализации этих платформ; сейчас готовится только Windows x64.

Во всех поставках указывать **© nr084, год сборки**; год вычисляется при сборке. Windows: поле LegalCopyright и «О программе» на обоих языках.

Финальные бинарники максимально сжимать последней stable-версией UPX (`--ultra-brute --lzma`), доступной на момент упаковки. Сверять релиз с официальным upstream, проверять `upx -t` и запуск упакованного приложения. Несжатый EXE сохранять отдельно для диагностики. Успешность упаковки и smoke-теста записывать в состояние конкретного milestone. Если UPX не поддерживает бинарник данной платформы или проверка не проходит, не представлять его как успешно упакованный.

## v0.2.0 win64

Проверен [официальный stable UPX 5.2.1](https://github.com/upx/upx/releases/tag/v5.2.1). Он отказался упаковывать EXE с GUARD_CF и предложил `--force` для отключения CFG. На том этапе CFG был сохранён, v0.2 ZIP содержит исходный EXE. Это историческое состояние; следующая инструкция пользователя изменила правило для v0.3.

## v0.3.0 win64

Пользователь явно указал удалить GUARD_CF и максимально сжать следующий EXE. `/guard:cf` убран из compiler/linker options; raw PE проверен на отсутствие Guard CF, ASLR/NX/HIGHENTROPYVA сохранены. EXE упакован stable UPX 5.2.1 с `--ultra-brute --lzma`; `upx -t` и smoke-тест packed приложения пройдены. Параметры, SHA-256 и размеры сохранены в `dist/release-info.json`.

Будущие значки: современные, с лёгким налётом 90-х. Эта установка закреплена в AGENTS.md; новый набор графики пока не создавался.

## v0.4.0 win64

Добавлены versioned UTF-8 профили, несколько диапазонов без повторного сканирования адресов, настраиваемые порты/таймауты/concurrency, CSV/TXT/HTML и GDI-статистика. Отчёт создаётся в фоне по снимку сессии с атомарной заменой назначения. Пройдено 7/7 CTest, включая RU/EN редактор, профильный backend и отмену файлового экспорта.

Последний stable UPX 5.2.1 проверен через официальный API и digest; `--ultra-brute --lzma` уменьшил EXE с 1060864 до 396800 байт (37.40%). Integrity и packed UI smoke пройдены. GUARD_CF отсутствует; ASLR/NX/HIGHENTROPYVA сохранены. Точные hash находятся в `dist/release-info.json`. Поставка остаётся тестируемым milestone Windows x64; далее — стабилизация v1.00.

## v0.4.1 win64 — стабилизация

DNS переведён на асинхронные forward/PTR запросы с отменой по Stop, generation и shutdown; лимит ожидания 3 секунды. Все backend workers штатно завершаются, handles/critical section освобождаются через UI timer. Отмена hostname scan не выдаётся за ошибку. Версия окна/About/ресурсов генерируется из CMake.

8/8 тестов прошли, включая deterministic pending-DNS cancellation/timeouts, 20 отмен hostname scan и 30 init/shutdown cycles без роста handles. Shutdown в интеграционном прогоне занял 16 ms; это измерение, не гарантия для любого окружения. Синхронный ARP и ожидание OS completion после отмены остаются ограничениями. Windows 10/11 и ручной DPI matrix ещё предстоят, stable v1.00 пока не заявлен.

UPX 5.2.1 `--ultra-brute --lzma`: 1063936 → 396800 байт (37.30%), ZIP 388092 байт. Integrity, packed UI smoke и SHA-256 содержимого ZIP проверены; параметры/hash актуальной версии — `dist/release-info.json`.

## v0.4.2 win64 — стабилизация ICMP

Source-bound ping переведён на асинхронный IcmpSendEcho2Ex с event. Stop освобождает worker от ожидания; pending request передаётся одному cleanup thread. Reply/payload/handles сохраняются до completion, active+retired requests ограничены 256. В простое cleanup thread спит без polling. При закрытии UI timer ждёт безопасного освобождения requests; scan errors во время closing не открывают диалог.

10/10 CTest passed: отдельные ICMP doubles проверяют sync/async/error, prompt cancel, отсутствие преждевременного освобождения, заполненный pool и delayed shutdown. 320 retire/reap cycles прошли без роста handles. Hidden UI close-scan и 15 backend shutdown после запуска coordinator также прошли. Синхронный ARP остаётся ограничением; стабильная v1.00 ещё не заявлена.

UPX 5.2.1 `--ultra-brute --lzma`: 1065472 → 398848 байт (37.43%), ZIP 389981 байт. Integrity, packed startup и packed close-scan прошли; версия/© nr084, 2026 и ZIP SHA-256 проверены. ASLR/NX/HIGHENTROPYVA сохранены, GUARD_CF отсутствует.

## Stable 1.0.0 win64

12 октября 2026 пользователь явно разрешил stable с перечислением непроверенных пунктов. Отдельная Windows 10/11 matrix, физические DPI monitors и длительный LAN stress остаются непроверенными; подробности в [RELEASE_CHECK.md](RELEASE_CHECK.md). Это исключение записано в AGENTS.md и не означает прохождение всех исходных DoD checks.

Добавлены bounded background ARP pool с отменой ожидания caller, общий probe pacing, 100-transition DPI/RU/EN/layout/GDI smoke, 253-byte hostname и /23 tests. Исправлены ширины toolbar и прозрачность status images. Встроен оригинальный SVG-derived ICO 16–256 px. Полный CTest: 12/12 passed; 500 warm-up + 500 measured self-host Scan/Stop без дальнейшего handle growth, затем штатный shutdown.

UPX 5.2.1 `--ultra-brute --lzma`: 1108480 → **438784 bytes** (39.58%), ZIP **429016 bytes**. Integrity, packed startup/DPI/close-scan passed. Только system DLL, static CRT, ASLR/NX/HIGHENTROPYVA; Guard CF отсутствует. Версия 1.0.0 и © nr084, 2026 проверены.

Artifact: `Look@LAN-ng-1.0.0-win64.zip`; metadata: `Look@LAN-ng-1.0.0-win64.json`. ZIP SHA-256: `553928A581863402CFB85DE203292E15F529B943C0E9E5EDBA01B072319D8F50`; EXE SHA-256: `0FACC8DE81EDC8DA9ED9FE1B5C3497986CA7FB6B6ED223D4A9F1066536474429`. Это локальная поставка, внешняя публикация не выполнялась.

## Stable 1.1.0 win64

Исправлено главное окно: readable disabled toolbar, font/DPI dimensions, keyboard focus/tooltips, компактный верхний блок; TreeView больше не отключается/не сереет во время scan, смена NIC защищена notification. Добавлены NetBIOS fallback, native Ping/history, native Trace с TTL 1–30, optional Open SSH и device notes текущей сессии. Таблица ТЗ: `SPEC_STATUS.md`; непроверенная Windows/NIC/LAN matrix сохраняется.

15/15 full CTest passed, 53.92 s; затем tools/UI checks 5/5 passed. Реальные own-IP Echo и one-hop traceroute, UI close-tools, core NBSTAT/notes/history и TTL/cancel doubles проверены.

UPX 5.2.1 `--ultra-brute --lzma`: 1120768 → **443392 bytes** (433 KiB, 39.56%); ZIP **433951 bytes**. Integrity и packed startup/DPI/close-scan/close-tools passed. ZIP содержит один проверенный EXE; metadata в `Look@LAN-ng-1.1.0-win64.json`.

Новый EXE: `dist/1.1.0/Look@LAN-ng.exe`. Flat `dist/Look@LAN-ng.exe` занят работающей 1.0 и не заменён; пользовательская сессия не закрывалась. `exe_path` и `latest_exe_updated=false` в JSON исключают смешение старого и нового binary. ZIP SHA-256: `940AD572FA4518BA19F58B69F584B134B453634BA2D3C472D9AFA030C8CE0FE0`.

## Stable 1.2.0 win64

Добавлены шесть оригинальных toolbar icons: радар, стоп, обновление, узел, мониторинг, экспорт. Геометрия сглаживается 4x supersampling, адаптируется к DPI; disabled и checked состояния сохраняются. RU/EN интерфейс переключается через View → Language, добавлен сохраняемый System language (default): русский Windows UI → русский, любой другой → английский. Явный выбор ru/en сохраняет приоритет; настройка записывается сразу.

15/15 CTest passed (53.82 s), финальные UI checks 4/4 passed (3.27 s), в том числе команды меню языка и отрисовка всех enabled/disabled glyphs на 100–200% DPI с GDI/USER bounds. RU/EN native sample previews проверены. Остаток ручной Windows/NIC/long-LAN matrix и optional IPv6 neighbor display остаётся отмеченным в SPEC_STATUS.md.

UPX latest stable 5.2.1, official API/digest: `--ultra-brute --lzma`, 1125376 → 445440 bytes (435 KiB, 39.58%), ZIP 436001 bytes. Integrity и packed startup/DPI/close-scan/close-tools passed. ZIP содержит один EXE с проверенным hash, версия 1.2.0 / © nr084, 2026. Flat EXE обновлён успешно, versioned path: dist/1.2.0/Look@LAN-ng.exe. Пользовательские процессы не завершались.

ZIP SHA-256: `AC036B10D70633FC5D889D3CC63B53DF24E0C194CFE3672352D1D656B3810E01`; EXE SHA-256: `03DD9B5CBE5DA0EB045752DD9AA3F56CC8A6A91AFDF6AA5FF2A9A3C646E4B52C`. Локальная поставка; внешней публикации нет.

## First GitHub release v1.2.0 — x86 / x64 / ARM64

12 октября 2026 опубликованы исходники в публичный https://github.com/nr084/Look-LAN-ng и первый GitHub release https://github.com/nr084/Look-LAN-ng/releases/tag/v1.2.0 . README и release notes на русском/английском. GitHub names не поддерживают @, имя приложения осталось Look@LAN-ng.

Одна архитектура — один ZIP с одним EXE: look@lan-ng-x86-1.2.0.zip (414358 bytes), look@lan-ng-x64-1.2.0.zip (436044 bytes), look@lan-ng-arm64-1.2.0.zip (511568 bytes). UPX 5.2.1 --ultra-brute --lzma; packed EXE 423936 / 445440 / 546816 bytes. Raw diagnostic binaries сохранены в CI artifacts и build/unpacked/<arch>.

45/45 CTest: x86 56.39 s, x64 51.09 s, native ARM64 45.05 s; 12/12 packed smoke modes, UPX integrity на всех трёх. CI: https://github.com/nr084/Look-LAN-ng/actions/runs/38256307207 . ASLR/NX on all; HIGHENTROPYVA only 64-bit; no Guard CF, static CRT/system DLL. Исправлен ARM64 optimized MSVC 2026 NBNS GROUP check; Release/Debug и high/low wire-byte regressions passed. ZIP скачаны обратно, digest/release-notes SHA-256/single EXE/machine/version проверены.

ZIP SHA-256 x86: 14EE61EA0AD2A036D4E16DE4DBC4043DF0D2A038ADF49FA6B2140ECC51D4845D
ZIP SHA-256 x64: A68FB16B3FE0D97D8537F60FC4260598CC4A27019D78B24BD5482809E983DEB6
ZIP SHA-256 ARM64: F259E84A318F8193FC4B341AD3A04972371664C2471DBEBEA88A5C7DFA35FB9D

Следующие v<version> tags автоматически публикуются после всех checks. Полная ручная Windows 10/11/NIC/physical DPI/long-LAN matrix по-прежнему не объявляется пройденной.
