## Русский

Look@LAN-ng 1.2.0 — компактный портативный LAN-сканер для Windows с интерфейсом на русском и английском.

- Современные значки toolbar с лёгкой ретро-отсылкой, поддержка DPI 100–200%.
- Выбор языка: язык системы (по умолчанию), English, Русский. Русский Windows UI → русский; любой другой → английский. Выбор сохраняется.
- Обнаружение IPv4-узлов через ARP/ICMP/TCP, DNS/NetBIOS-имена, MAC/OUI, TCP-порты, мониторинг, Ping/Trace, профили и CSV/TXT/HTML.
- Три отдельных ZIP: `look@lan-ng-x86-1.2.0.zip`, `look@lan-ng-x64-1.2.0.zip`, `look@lan-ng-arm64-1.2.0.zip`. Каждый содержит один EXE для своей архитектуры.
- Сборка и автоматические тесты для каждой архитектуры; UPX latest stable, максимальное сжатие, integrity и packed smoke tests. SHA-256 ZIP опубликованы ниже.

Не выполнена полная ручная проверка Windows 10/11, разных физических адаптеров/мониторов, реальных сетевых отказов и длительного мониторинга. Optional IPv6 neighbor display и будущие Linux/macOS поставки не реализованы. Подробности: [SPEC_STATUS](docs/SPEC_STATUS.md), [RELEASE_CHECK](docs/RELEASE_CHECK.md).

## English

Look@LAN-ng 1.2.0 is a compact portable Windows LAN scanner with Russian and English interfaces.

- Modern toolbar icons with a subtle retro influence; DPI 100–200% support.
- Language selection: system language (default), English, Русский. Russian Windows UI selects Russian; all other languages select English. Your choice is saved.
- IPv4 discovery via ARP/ICMP/TCP, DNS/NetBIOS names, MAC/OUI lookup, TCP ports, monitoring, Ping/Trace, profiles and CSV/TXT/HTML export.
- Three separate ZIPs: `look@lan-ng-x86-1.2.0.zip`, `look@lan-ng-x64-1.2.0.zip` and `look@lan-ng-arm64-1.2.0.zip`. Each contains one executable for its architecture.
- Each architecture is built and automatically tested; latest stable UPX with maximum compression, integrity and packed smoke tests. ZIP SHA-256 checksums are published below.

The full manual Windows 10/11, physical adapter/monitor, real network failure and long-running monitoring matrix remains untested. Optional IPv6 neighbor display and future Linux/macOS distributions are not implemented. See the repository's SPEC_STATUS and RELEASE_CHECK documents for details.
