# Профили v2 / Profile format v2

Меню «Файл» позволяет создать, открыть, изменить и сохранить `.lanprofile`. Редактор работает на русском и английском. Cancel не меняет текущий профиль. Открытие/создание другого профиля переключает его историю; редактирование диапазонов текущего профиля сохраняет известных хостов. Сканирование запускается явно.

Формат: UTF-8, допускается BOM, максимум 64 KiB. Пример:

```ini
Look@LAN-ngProfile=2
name=Office
range=192.168.1.1-254
range=192.168.2.0/24
ports=22,80,443,445,3389
icmp_timeout_ms=300
tcp_timeout_ms=250
monitor_interval_sec=30
concurrency=64
notes=First line\nSecond line
```

`interface_id` и `interface_name` сохраняют выбранный адаптер. Идентификатор используется для повторного выбора NIC; если адаптер недоступен, программа требует выбрать интерфейс вручную. Выбор другого NIC явно привязывает профиль к нему.

`range` может повторяться до 64 раз. Допускаются одиночный IPv4, полный диапазон, сокращённый последний октет и CIDR. CIDR до /30 исключает network/broadcast; /31 и /32 сохраняют все адреса. Пересекающиеся и соседние диапазоны объединяются перед сканированием. Monitoring учитывает только адреса объединения, без промежутков между диапазонами. Сканирование более 4096 адресов требует подтверждения; более INT32_MAX адресов отклоняется.

TCP-порты задаются числами 1–65535 и диапазонами через запятую; список сортируется и очищается от дублей. ICMP timeout: 50–5000 ms; TCP: 25–5000 ms; concurrency: 8–256. Monitoring interval: 5, 10, 30, 60 или 300 секунд после завершения предыдущего цикла. Monitoring использует максимум 16 workers без повторного service scan. Большой список TCP-портов требует подтверждения.

Имя ограничено 255 байтами UTF-8, заметки — 2047 байтами. В текстовых полях сериализация поддерживает `\n`, `\r`, `\\`. Неверный UTF-8, неизвестные ключи, повторные одиночные ключи, неверная версия и значения отклоняются без изменения текущего профиля.

Папка по умолчанию — `profiles` рядом с EXE; если её нельзя создать, используется `%LOCALAPPDATA%\Look@LAN-ng\profiles`. Она создаётся только при сохранении. До 32 корректных сохранённых профилей показываются в дереве.

CSV/TXT/HTML экспорт содержит снимок всех записей сессии, независимо от фильтра. CSV имеет UTF-8 BOM и защиту текстовых ячеек от формул; HTML экранирует поля и не использует внешние ресурсы. Время указано в UTC. Экспорт пишет временный файл рядом с назначением и заменяет итоговый файл после успешной записи; отмена или ошибка сохраняет прежний файл. Windows-путь отчёта ограничен MAX_PATH минус запас для временного суффикса (48 символов).

## Постоянный режим / Persistence (1.4.0)

Разовый запуск и сканирование не создают INI/каталоги настроек. Успешное сохранение профиля активирует постоянный режим; открытие уже сохранённого профиля также восстанавливает этот режим. В profiles выбирается единственный корректный профиль v2; при нескольких автоматического выбора нет. Настройки UI сохраняются только в постоянном режиме. В Linux каталог profiles находится рядом с исполняемым файлом, в Mac — в Application Support/LookLAN-ng; macOS native validation выполняется отдельно на Mac.

В Windows, Linux и macOS сохранение направляет выбранное имя файла в управляемую папку profiles. Старый формат v1 отклоняется без преобразования и перезаписи. Старые файлы автоматически не удаляются.

Temporary launches/scans do not create INI/settings directories. A successful save or opening an existing saved profile activates persistence. One valid v2 profile is selected on startup; multiple profiles require explicit selection. UI settings are written only in persistent mode. Device notes, Ping samples and the last Trace remain session-only. Profile notes remain in .lanprofile.

## История / History format 1

`Office.lanprofile` and `Office.lanprofile.lanhistory` form one logical profile. Back up or move both; the history file is not required to begin a new history. No SQLite/runtime dependency is required. History belongs to the saved interface ID; runtime interface indices are rebound when loading on the same interface. Switching to a different interface starts a separate session; foreign-interface history is not silently merged.

History stores up to 65536 host records and a rolling 10000-event log. It preserves IPv4, host name, MAC, OS hint, TCP ports, first/last seen, prior online/cache state and NEW/CHANGED acknowledgement flags. Events preserve sequence, timestamp, type, address, previous and current values. This is a bounded snapshot, not an unlimited archive. Export reports separately when a permanent historical archive is needed.

Restored rows show HISTORY/ИСТОРИЯ and do not count as currently ONLINE. Confirmed replies clear this flag; two completed misses can establish OFFLINE. The prior baseline survives restart, so genuinely new hosts and MAC/name changes remain detectable. First seen and event sequence continue across launches. Monitoring must be started explicitly; application startup does not silently scan the network.

Wire format is independent of compiler, architecture and C structure padding; all integers are little-endian:

| Part | Bytes | Contents |
| --- | --- | --- |
| Header | 176 | LANHIST1 magic, uint32 version=1, host/event counts, baseline, uint64 round and sequence, 128-byte interface ID, reserved zeros |
| Host | 448 each | IPv4, 256-byte UTF-8 name, MAC, flags, OS/confidence/TTL, up to 64 uint16 ports, first/last-seen timestamps, reserved zeros |
| Event | 552 each | uint64 sequence/time, uint32 type/host index/IPv4, two 256-byte UTF-8 value fields, reserved zeros |
| Footer | 4 | IEEE CRC-32 of every preceding byte |

Header offsets: version 8, hosts 12, events 16, baseline 20, round 24, sequence 32, interface ID 40. Host offsets: IPv4 0, name 4, MAC 260, flags 266, OS/confidence/TTL/port count 268–271, first seen 272, last seen 280, ports 288. Event offsets: sequence 0, time 8, type 16, index 20 (FFFFFFFF = global event), IPv4 24, previous 28, current 284.

File I/O runs on an immutable background snapshot. A temporary sibling is flushed and atomically replaces the destination only after a successful write. Save occurs after scan/monitoring changes (batched to about 5 seconds between rounds) and on clean close. Loading validates version, counts, lengths, CRC, interface identity, record bounds and event sequence before committing to the model. Corrupt/unsupported history leaves the file intact, reports an error and disables automatic writes for that session. Save under another profile name to begin a fresh history without replacing it. Abrupt termination can lose changes since the last completed snapshot.
