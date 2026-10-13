# Выполнение ТЗ — 1.2.0

Пункты относятся к `SPECIFICATION.md` с пользовательскими уточнениями имени, RU/EN, copyright, GUARD_CF и UPX. Functional implementation и проверки разделены. Разрешение пользователя выпустить stable с оговорками не выдаётся за прохождение всей validation matrix.

| Пункты ТЗ | Состояние |
| --- | --- |
| 1–6: portable C17/native Win32/style/table | Реализовано; RU/EN, original icon, compact UI, sortable ten columns |
| 7–12: NIC/ranges/cache/ARP/ICMP/TCP | Реализовано; source binding, bounded pools, progressive results |
| 13: reverse DNS/NetBIOS/cache | Реализованы DNS→NBSTAT fallback и session cache; mDNS прямо отнесён ТЗ к будущему |
| 14–16: vendor/OS/common/custom ports | Реализовано; вероятностные OS hints, без точного fingerprinting |
| 17: General/Ports/Ping/Trace/History, notes | Реализовано; device notes/session ping history, native background Trace |
| 18: Ping/Traceroute/Web/HTTPS/RDP/SSH/SMB/copy/rescan/properties | Реализовано; SSH только при наличии Windows OpenSSH client |
| 19–24: Single Host/monitoring/NEW/log/statistics/tree | Реализовано; bounded memory, history current session |
| 25–27: UTF-8 profiles/CSV/TXT/HTML/portable settings | Реализовано; autonomous HTML, transactional save/export |
| 28–32: size/build/system APIs/concurrency | Реализовано; static CRT, x86/x64/ARM64, only system DLL, 8–256, cancellation/rate cap; GUARD_CF удалён по уточнению пользователя |
| 33–35: portable boundaries/future platforms | Core без Windows headers; Windows x86/x64/ARM64 опубликованы; Linux/macOS остаются будущими |
| 36: IPv6 | Address model поддерживает family/scope; active IPv4. Optional neighbor display не реализован; массовый IPv6 scan запрещён ТЗ |
| 37–39: model/ownership/errors | Bounded arrays/rings/queues, explicit lifetime и error handling; автоматические tests |
| 40–43: Cancel/responsiveness/rate/privileges | Scan и diagnostics cancellation; UI не ждёт workers. Native IO завершает safe cleanup позже; жёсткий OS shutdown deadline не гарантируется |
| 44: excluded features | Capture/cloud/accounts/driver/service/updater/embedded remote clients не добавлены |
| 45–48: milestones | Реализованы последовательно |
| 49–51,57: stabilization/DoD/resource validation | Automated tests и local integration выполнены; полная Win10/11/NIC/DPI/long-LAN matrix **не проверена** |
| 52–56,58: logging/UI/code/docs/workflow | Native background pipeline, no release debug log, docs/screenshot/tests; цели выполнены с указанными validation gaps |

Новые функции 1.1 закрывают ранее отсутствовавшие NetBIOS, Trace, standalone Ping/history, Open SSH и device notes. Device notes: до 511 UTF-8 bytes для 1024 IP/interface keys; clearing notes освобождает slot. Общая ping history: 10000 samples; выбранная панель показывает до 60 последних. Trace: один запрос на TTL, максимум 30, последний маршрут текущей сессии. Эти данные не записываются на диск; профильные notes сохраняются отдельно.

Из проверки вне текущего окружения остаются полная ручная Windows 10/11 matrix, physical DPI monitor moves, полная NIC/Wi-Fi/Ethernet/offline/ICMP-blocked/DNS-unavailable matrix и длительный monitoring/memory/socket stress. Подробности: `RELEASE_CHECK.md`.


В 1.2 добавлены оригинальные геометрические toolbar icons (radar/stop/refresh/host/monitor/export), сглаженные supersampling 4x и масштабируемые вместе с DPI. Подписи, отключённые состояния и keyboard focus сохранены. В узком окне фильтр переносится на вторую строку.

Язык: **Вид → Язык / View → Language** → **Язык системы / System language (default)**, **English**, **Русский**. По умолчанию язык определяется по Windows UI language: русский → русский, любой другой → английский. Выбор применяется сразу и записывается в INI как `system`, `ru` или `en`; system mode остаётся автоматическим при следующих запусках. Ранее сохранённый явный выбор ru/en сохраняется. Profile editor, меню, таблица, вкладки, статистика, ошибки и диалоги используют выбранный язык; пользовательские имена/данные не переводятся.

GitHub release v1.2.0 опубликован с тремя ZIP, каждый содержит один architecture-specific EXE. 45/45 CTest и 12/12 packed smokes passed; ARM64 native Windows 11, x86/x64 Windows Server 2022. NetBIOS GROUP flag проверяется непосредственно в wire byte; Release/Debug ARM64 и high/low byte regression passed. CI: https://github.com/nr084/Look-LAN-ng/actions/runs/38256307207 .

## Unix candidate status — 13 October 2026

Windows implementation/validation above describes the existing Windows release; it is not a claim of native Mac validation. Linux/macOS now share the missing DNS/NetBIOS, Ping/Trace, automatic monitoring, Single Host, profile editor/remapping, context actions, session notes/history/log/statistics and asynchronous transactional file I/O. Native network/profile sidebars and General/Ports/Ping/Trace/History/Log tabs are present. Monitoring no longer repeats service scans; scan Ping samples enter history. Default concurrency is 64 with configurable 8–256.

Linux Ubuntu 26.04.1 x64: six native C suites, scoped DNS/cache/generation tests, timed monitoring, diagnostics, file snapshots/cancellation, fresh-ARP and static-cache fixtures, Stop/FD checks, and 38 GTK workflows passed on the private candidates. Exact final candidate artifact results and limitations are in docs/LINUX_VALIDATION.md and local build/validation reports. Ubuntu 22.04 is a cross-build baseline, not a tested runtime. Physical NIC unplug, physical HiDPI displays, live non-ASCII NBNS and hours-long LAN load are not claimed as tested.

macOS universal x86_64/arm64 compiles locally; code page/resource seals and DMG hashes/execute modes pass. User-confirmed port.1 startup/LAN scan is historical evidence only. New Cocoa layout, fresh routing notifications, diagnostics and full candidate workflows need user native results. Native-tests.command and the manual checklist accompany the local candidate.

The candidate is withheld from GitHub. Both Linux and macOS stable publication are gated on completion and exact-artifact native evidence. Future mDNS, active IPv6 and persistent disk history remain outside the current release scope. Non-ASCII OEM NetBIOS names are shown as unknown rather than guessed. Final Windows/Linux screenshots are captured only from final release binaries; user supplies macOS.
