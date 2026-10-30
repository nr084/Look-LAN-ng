"""Strong deterministic screenshot redaction; review every output before publishing.

Install Pillow in build/native-tools/image-tools (kept local). Rectangles are
pixel coordinates from a fresh screenshot and include every address-bearing
area. This script strips metadata and never uploads the unredacted original.
"""
import argparse, hashlib, json, sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'build/native-tools/image-tools'))
from PIL import Image, ImageFilter
p=argparse.ArgumentParser(description=__doc__);p.add_argument('input',type=Path);p.add_argument('output',type=Path);p.add_argument('--rect',action='append',required=True);a=p.parse_args()
im=Image.open(a.input).convert('RGB');rects=[]
for raw in a.rect:
    box=tuple(map(int,raw.split(',')))
    if len(box)!=4 or not (0<=box[0]<box[2]<=im.width and 0<=box[1]<box[3]<=im.height):raise SystemExit('Invalid privacy rectangle')
    # Downsample before strong blur so address glyphs cannot remain readable.
    crop=im.crop(box);crop=crop.resize((max(1,crop.width//32),max(1,crop.height//32)),Image.Resampling.BOX).resize(crop.size,Image.Resampling.BILINEAR).filter(ImageFilter.GaussianBlur(28));im.paste(crop,box);rects.append(box)
a.output.parent.mkdir(parents=True,exist_ok=True);im.save(a.output)
print(json.dumps({'file':a.output.name,'size':im.size,'rectangles':rects,'method':'32x downsample + bilinear + Gaussian radius28; metadata stripped','sha256':hashlib.sha256(a.output.read_bytes()).hexdigest()}))
