import sys
from pathlib import Path
source, target = map(Path, sys.argv[1:])
data = source.read_bytes()
target.parent.mkdir(parents=True, exist_ok=True)
with target.open('w', encoding='ascii', newline='\n') as output:
    output.write('#include <stddef.h>\nconst unsigned char desktop_oui[]={\n')
    for at in range(0, len(data), 32):
        output.write(','.join(str(x) for x in data[at:at+32])+',\n')
    output.write('};\nconst size_t desktop_oui_size=sizeof(desktop_oui);\n')
