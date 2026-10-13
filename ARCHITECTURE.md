# Архитектура

## Переносимый core

`src/core` не включает Windows headers. `network` хранит IP-адреса с семействами IPv4/IPv6 и scope ID, разбирает IPv4 через указатель и явную длину, рассчитывает границы подсети, включая /31 и /32. `host` объединяет известные поля устройства без потери MAC при DNS-обновлении. Ключ устройства: адрес, scope ID и интерфейс. Владельцем памяти таблицы является frontend.

`scanner` выполняет ARP → ICMP через `platform_net_ops_t`, проверяя cancellation между этапами. ARP-ответ достаточен для discovery при отсутствии ICMP. Wall-clock время используется для First/Last Seen; длительность сканирования измеряется Windows monotonic clock. Fake backend проверяет эту логику без Win32.

v0.2 добавляет TCP fallback в platform ops. SCAN_OK и SCAN_REFUSED подтверждают presence; только SCAN_OK добавляет открытый порт. Host discovery публикуется до service scan. `ports` содержит bounded pointer/length parser с deduplication и quick/discovery sets; `vendor` валидирует compact binary table и делает O(log N) lookup. Host merge сохраняет service enrichment при позднем DNS update.

## Windows backend

`src/platform/windows/win_net.c` перечисляет интерфейсы через GetAdaptersAddresses и уточняет физический статус через GetIfEntry2. Активные виртуальные/APIPA/tunnel-интерфейсы допускаются только при явном выборе. При запуске пользователь должен нажать Scan; автоматического активного сканирования нет.

Coordinator сначала читает GetIpNetTable2. Затем 8–256 постоянных для одного scan workers (default 64) получают адреса из атомарного счётчика: отдельного потока на каждый IP нет. ResolveIpNetEntry2 привязан к index/source IPv4 выбранного интерфейса и обновляет neighbor вместо использования stale MAC как liveness evidence. IcmpSendEcho2Ex использует тот же source IPv4 и timeout профиля (default 300 ms). Coordinator поочерёдно ожидает и закрывает thread handles, без лимита WaitForMultipleObjects и ожидания в UI. Own-host без шестибайтового MAC остаётся online с неизвестным MAC.

В v0.4.2 `win_ping` отправляет ICMP асинхронно с event, получает результат через IcmpParseReplies и проверяет cancel каждые ≤25 ms. Payload/reply/event/ICMP handle принадлежат heap request. На Stop worker передаёт pending request единственному cleanup thread и возвращает SCAN_CANCELLED. Cleanup освобождает storage только после event completion; pending ICMP не прерывается закрытием opaque handle. Pool ограничен 256 активными и retired requests вместе, поэтому новые scans не накапливают неограниченные handles. При заполнении pool новый probe ждёт место с проверкой cancel. Cleanup thread спит без polling при отсутствии retired requests, при наличии проверяет completion каждые 25 ms. Shutdown прекращает выдачу запросов и ждёт их completion через UI timer, без блокировки UI. Для неотвечающего ping это может занять оставшуюся часть профильного timeout (до 5 s); жёсткий срок OS completion не обещается.

Контракты ICMP: [IcmpSendEcho2Ex](https://learn.microsoft.com/en-us/windows/win32/api/icmpapi/nf-icmpapi-icmpsendecho2ex), [IcmpParseReplies](https://learn.microsoft.com/en-us/windows/win32/api/icmpapi/nf-icmpapi-icmpparsereplies).

Discovery results поступают в ограниченную очередь из 2048 событий под critical section. Полная очередь создаёт backpressure только в workers. В UI нет сетевых запросов. Timer каждые 50 ms забирает до 256 событий, изменяя только нужные строки. Идентификатор поколения исключает результаты предыдущего scan. Таблица ограничена 65536 найденными устройствами.

Reverse DNS имеет отдельную очередь до 4096 заданий и четыре workers. В v0.4.1 `win_resolver` использует DnsQueryEx/DnsCancelQuery для PTR с interface index. Predicate проверяет Stop, shutdown и generation каждые ≤25 ms; лимит ожидания 3000 ms. Старые задания отменяются при смене generation. DNS не задерживает scan completion и не обращается к HWND. Query/result/event остаются живы до OS completion после запроса отмены; это предотвращает use-after-free. Лимит ожидания не обещает жёсткий срок возврата callback из Windows.

## Native frontend

В 1.1 `core/nbns` создаёт wildcard NBSTAT query и разбирает ответы по RFC 1002 с bounded labels/RDLENGTH/name table, проверкой transaction, flags, truncation и group/workstation suffix. `win_nbns` использует connected nonblocking UDP socket, source bind, 300 ms deadline и generation/cancel predicate ≤25 ms. Он вызывается в существующих четырёх resolver workers только после неудачного PTR. Request source копируется в DNS task, поэтому новая NIC-сессия не меняет источник старого запроса. Стандарт: [RFC 1002](https://www.rfc-editor.org/rfc/rfc1002).

`win_ping_probe_ttl` сохраняет IP_OPTION_INFORMATION в heap request до completion и возвращает reply address/status/RTT отдельно от success Echo. TTL exceeded не объявляется успешным ping или ONLINE. `win_tools` владеет одним cancellable worker и максимум 30 result samples: четыре Ping либо TTL 1–30 Trace; result queue защищена коротким lock. UI забирает samples по timer, проверяет session ID, затем закрывает thread handle. Закрытие окна ждёт tools job до освобождения shared ICMP pool.

`core/details` содержит bounded global ping ring 10000 и notes dictionary 1024, ключ IP/interface. Ping samples не меняют monitoring liveness. Automatic NET_PING events отделены от NET_HOST/NET_DNS. General показывает device notes; Trace хранит результаты последнего запуска, Ping фильтрует samples по selected host. Reset session cancels job, увеличивает session ID и очищает детали; late results не попадают в новую сессию.

Toolbar остаётся native common control; NM_CUSTOMDRAW рисует читабельный disabled text и hover/focus state без disabled-text clipping. Font metrics задают button width/height. TreeView остаётся enabled; TVN_SELCHANGING запрещает смену NIC во время scan/monitoring. Open SSH запускает проверенный System32/OpenSSH/ssh.exe с числовым IP и restricted username; shell command strings/credentials не используются и не сохраняются.

В 1.0.0 fresh ARP вынесен в `win_neighbor`: 16 process-owned workers и до 256 heap requests. Задание копирует source/target/interface, не использует изменяемые поля следующего scan. Caller ждёт event с проверкой cancel ≤25 ms; queued request удаляется при cancel, running request помечается abandoned и освобождается worker после ResolveIpNetEntry2. Late result не публикуется и не может повлиять на новый generation. Native API не прерывается насильно. Shutdown будит workers, прекращает новую выдачу и ждёт их завершения через UI timer. Отдельного потока на каждый IP нет.

Общий scheduler резервирует monotonic tickets под коротким lock, затем ждёт вне lock с проверкой cancel. Initial probes: ticket spacing 4 ms (~250/s); monitoring: 16 ms (~62.5/s). Own-host ARP shortcut и запросы без маршрута не создают сетевого traffic. Ограничение распространяется на ARP, ICMP и TCP; ARP OS retries внутренние, не управляются scheduler.

`win_main.c` содержит Win32 окно, native toolbar, TreeView, ListView, TabControl, StatusBar и progress control. Единственный UI thread владеет controls и таблицей host model. Меню и текст поддерживают русский/английский. Манифест задаёт PerMonitorV2 и asInvoker. Размеры layout выражены в logical pixels и масштабируются по DPI окна; системный шрифт обновляется при WM_DPICHANGED. Зелёная/серая точка статуса рисуется собственным GDI-кодом.

Toolbar widths вычисляются по текущему font/text и задаются явно. В узком окне filter переносится под кнопки. Status images — premultiplied BGRA DIB с supersampled alpha, без magenta mask/black-box артефактов. Оригинальный icon встроен в ICO 16–256 px; SVG и воспроизводимый генератор лежат в repository. DPI smoke проверяет 100 transitions на 96/120/144/168/192 DPI, RU/EN, двух размерах окна, bounds/overlap и GDI/USER counts. Это synthetic WM_DPICHANGED, не physical-monitor matrix.

Core не зависит от UI или Windows event queue. Для Linux/macOS потребуется другой platform backend и native frontend; address/model/probe policy сохраняются.

Windows TCP backend использует nonblocking Winsock connect, source bind к выбранному интерфейсу и select с проверкой cancel каждые ≤50 ms. Service probes выполняются последовательно внутри host worker, сохраняя concurrency профиля. Single Host forward lookup использует GetAddrInfoExW с manual-reset OVERLAPPED event, GetAddrInfoExCancel и тем же лимитом/predicate; numeric IPv4 обходит DNS. Ошибка отменённого lookup не публикуется как NET_ERROR.

При закрытии `win_backend_shutdown` устанавливает token и будит DNS workers. UI timer продолжает обработку до завершения coordinator, resolver workers и export. `win_backend_release` проверяет handles без ожидания, затем закрывает coordinator/worker/wake handles и critical section. API допускает повторные init/shutdown cycles; частичный отказ создания DNS workers тоже очищается. Полная очередь перестаёт блокировать producer при shutdown.

Контракты API: [GetAddrInfoExW](https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfoexw), [DnsQueryEx](https://learn.microsoft.com/en-us/windows/win32/api/windns/nf-windns-dnsqueryex), [DnsCancelQuery](https://learn.microsoft.com/en-us/windows/win32/api/windns/nf-windns-dnscancelquery).

В v0.2 frontend добавляет фильтр по отображаемым полям, Ports details и native input dialog. Host action URLs/UNC/RDP parameters формируются из проверенного числового IPv4 модели. Hostnames не интерполируются в shell commands. Явное действие пользователя вызывает системный browser, mstsc или SMB frontend через ShellExecuteW.

## Monitor core и события v0.3

`core/monitor` владеет логикой session state, caller предоставляет host array и ring storage. Полный успешно завершённый цикл увеличивает missing counter только для IP в его scope/interface. Два пропуска переводят подтверждённый узел в OFFLINE; cancel/error не увеличивает counters. Новый подтверждённый адрес после baseline получает NEW, MAC/name changes — CHANGED. Отдельный rescan имеет scope только одного IP, не затрагивая остальные строки.

DNS update отделён от confirmed discovery: он может изменить имя, но не seen-round, last-seen, MAC, online status или сервисы. Cached entries могут появляться как UNKNOWN; для старых устройств они не подтверждают присутствие. First/Last Seen сохраняются между scans того же выбранного интерфейса. Host model остаётся address/interface-oriented; IP CHANGED выводится эвристически, когда старый IP OFFLINE и существует ровно один активный адрес с тем же глобальным MAC. Старый offline record остаётся в истории; ambiguous/shared/local MAC не объединяются.

В monitoring backend concurrency ограничена 16, fresh ARP/ICMP обходят выбранные диапазоны для новых устройств, TCP fallback работает только для ранее подтверждённых IP, без повторного service scan. Перед round проверяется наличие interface/source IPv4: потеря интерфейса останавливает monitoring с log error, не объявляя все устройства исчезнувшими. Интервал отсчитывается от completion; rounds не пересекаются. Modal input/confirmation dialogs задерживают запуск новых monitoring rounds.

## Профили и отчёты v0.4

`core/network` разбирает до 64 диапазонов и строит сортированное объединение без дублей; uint64 totals исключают переполнение /0. Workers получают адрес по индексу объединения. Monitor scope использует то же объединение, не затрагивая промежутки между диапазонами.

`core/profile` валидирует versioned UTF-8, читает конфигурацию транзакционно и сериализует через writer callback. `win_profile_ui` содержит native RU/EN редактор; стабильный GAA adapter ID связывает профиль с NIC. `core/report` потоково пишет CSV/TXT/HTML с экранированием и UTC; `core/statistics` считает состояния и online OS families без UI зависимостей.

`win_files` использует Unicode W API. Export job владеет неизменяемым heap snapshot, отдельным worker и буфером 64 KiB. Запись идёт в уникальный sibling temp, FlushFileBuffers и MoveFileEx завершают замену. Cancel/error удаляет temp и сохраняет назначение. Закрытие отменяет IO через CancelSynchronousIo и завершает работу по UI timer. Статистика рисуется простыми GDI bars; UI thread владеет её моделью.

Журнал — ring из максимум 10000 структур с sequence, timestamp, event type, host index и previous/current details. UI добавляет только новые записи в отдельный native ListView; History берёт события выбранного stable row index из этой же ring. State полностью принадлежит UI thread; worker events проходят через существующую bounded queue. Смена NIC начинает новую сессию.

Toolbar glyphs: win_toolbar_icons.c, original GDI geometry rendered at 4x then HALFTONE downsample. Every temporary DC/bitmap/pen/brush is released per draw; DPI smoke renders all six enabled/disabled glyphs over 100 transitions and checks GDI/USER resource counts. UI language preference system/ru/en is separate from resolved russian flag; system follows GetUserDefaultUILanguage at startup and explicit menu selection persists immediately.

## Unix ports: local 1.3.0 candidate

The shared POSIX controller owns the host model, monitoring, details, profiles and reports on the UI thread. Linux uses GTK 3; macOS uses Cocoa/SF Symbols. Both expose interface/profile trees, numeric sorting/filtering, six detail tabs, profile editing, diagnostics, statistics, notes and native context actions. Linux rows retain model indices and receive incremental updates; Cocoa uses native table virtualization. Geometry/columns and RU/EN/system preferences are saved locally.

`posix_net` binds all probes to the chosen source IPv4 and interface (Linux IP_UNICAST_IF, Darwin IP_BOUND_IF). Loopback destinations/resolver stubs are local OS endpoints. Discovery has a bounded 8–256 worker setting (default 64), a 512-event queue, 65536-address limit and shared pacing: initial rounds 4 ms/probe, monitoring 16 ms. Monitoring repeats discovery without repeating configured service scans. Missing-interface/infrastructure errors prevent false OFFLINE transitions and stop automatic rounds.

`posix_neighbors` subscribes to kernel neighbor notifications before probing. Ordinary source-bound UDP requests trigger kernel ARP; Linux netlink REACHABLE notifications and Darwin routing messages supply fresh MAC evidence. Existing cache entries are enrichment only and remain UNKNOWN. A previously ARP-only host with only cached evidence becomes UNKNOWN rather than falsely OFFLINE. Linux fresh/static-cache fixtures with all ICMP/TCP/UDP replies blocked were tested without elevated scanner privileges. Darwin route behavior still requires native verification.

`posix_names` performs bounded, cancellation-aware reverse DNS then NBSTAT. Two persistent providers copy interface/IP/generation into each request and never block primary scan completion. The 1024-entry session cache includes source/interface/MAC and expiration; changed MACs invalidate reuse. Old-generation events and mismatched-MAC enrichment cannot overwrite a newer scope/device. PTR/A packet bounds, compression pointers, transaction/question/type/class and NBNS replies are validated. Non-ASCII OEM NetBIOS names remain unknown; mDNS is future work.

`posix_tools` runs four Ping samples or bounded 30-hop Trace in one worker. Linux Trace uses UDP/IP_RECVERR; Darwin delegates to the system traceroute with explicit argv/source/interface, C locale, bounded output and kill/wait cancellation. Darwin traceroute requires integer seconds (minimum one); timeout is rounded up, and the cancellation deadline uses the same value. Reference: [Apple traceroute source](https://github.com/apple-oss-distributions/network_cmds/blob/main/traceroute.tproj/traceroute.c). Ping results from scan rounds also enter the 10000-sample ring. Notes and logs are session-only.

`desktop_io` owns immutable export/profile snapshots. Workers parse bounded UTF-8 profiles, list up to 64 managed profiles, and write same-directory temporary files followed by flush/fsync/atomic rename. Cancellation removes owned temporaries and preserves the destination before commit. Missing imported interfaces require explicit user reassignment; failed parsing leaves current settings intact. UI close cancels/reaps all jobs through timers and releases model storage only after joins.

Compilation/packaging are local on Windows. Linux ELF uses PIE, full RELRO and a non-executable stack, then maximum stable UPX and native packed tests. Universal Mach-O retains ad-hoc code/resource seals when stable UPX rejects the format; DMG uses HFS+/LZMA with extracted hash/mode validation. Publication requires native evidence matching the final artifact; no port previews are published.
