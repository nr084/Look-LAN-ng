# IEEE OUI / MA-L lookup snapshot

Source: [IEEE Registration Authority public MA-L CSV](https://standards-oui.ieee.org/oui/oui.csv), downloaded 12 October 2026. Registry information: [IEEE Registration Authority](https://standards.ieee.org/products-programs/regauth/).

The organization names are retained from IEEE public assignment records. They are identification hints, not device-model or OS assertions. No registration logos or original Look@LAN assets are included.

`oui.bin`: 40316 MA-L prefixes, 787947 bytes. SHA-256: `84743A91F966B0B0F36BBB98EC1E09202878670183E4E7D43385AD1A04B59DCE`.

Format: `OUI1` magic, uint32 little-endian record count, uint32 string-table size, sorted pairs of uint32 prefix and uint32 UTF-8 string offset, then deduplicated null-terminated organization names. Core validates sizes, ordering, offsets and termination before binary search.

Locally administered/randomized MACs and multicast addresses are not assigned a vendor by this lookup. MA-M/MA-S registries are not included in this snapshot.

To regenerate offline after separately downloading IEEE CSV:

```powershell
.\tools\generate_vendor.ps1 -InputCsv path\to\oui.csv -OutputBinary .\data\oui.bin
```

The original CSV is build input only; it is not embedded or required at runtime. Update this snapshot description/hash when replacing the database.
