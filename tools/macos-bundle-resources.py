"""Install original icon and local-network permission translations in a Mac bundle."""
import struct
import sys
from pathlib import Path
root = Path(__file__).resolve().parents[1]
resources = Path(sys.argv[1]) / 'Contents/Resources'
resources.mkdir(parents=True, exist_ok=True)
png = (root / 'src/resources/assets/app.png').read_bytes()
chunk = b'ic08' + struct.pack('>I', 8 + len(png)) + png
(resources / 'LookLAN.icns').write_bytes(b'icns' + struct.pack('>I', 8 + len(chunk)) + chunk)
for language, message in [('en', 'Discover devices on the network interface you select.'), ('ru', 'Поиск устройств в сети на выбранном вами сетевом интерфейсе.')]:
    directory = resources / (language + '.lproj')
    directory.mkdir(exist_ok=True)
    (directory / 'InfoPlist.strings').write_text('"NSLocalNetworkUsageDescription" = "' + message + '";\n', encoding='utf-8')
