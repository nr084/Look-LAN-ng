# IEEE MA-L snapshot

Source: [official IEEE CSV](https://standards-oui.ieee.org/oui/oui.csv). Retrieved 2026-10-30T12:43:36.316318+00:00. 40421 prefixes, 790792 bytes. Binary SHA-256: `1bba0ae02f0e2d2e783c9a253a7ffab8f795fe1f2c6d91533d12c3499b432a5c`. Source CSV SHA-256: `6ef6e160740fda9ba94be57484d7a333eb091543c809437b443db1915d2fed93`.

Every release build refreshes this database through tools/build_preflight.py; failures stop the build. Provenance is in oui-source.json and each target's build preflight report. Runtime stays offline and uses embedded data only. No IEEE logos are included. Vendor names are assignment hints, not a device-model or OS assertion.

Format: OUI1 magic, uint32 LE count/string-table size, sorted uint32 prefix/UTF-8 string offsets, deduplicated null-terminated names. Core validates bounds, ordering and string termination. Locally administered/randomized and multicast addresses are not attributed. MA-M/MA-S registries are not included. The CSV is a build input, not a runtime dependency.
