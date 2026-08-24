#!/usr/bin/env python3
"""Split dictionary_zh.json into:
  - SpeedTree UI entries (kept in translations/dictionary_zh.json)
  - non-SpeedTree third-party / camera / image-format / code-internal strings
    (moved to tools/scratch/third_party_internal.json, NOT loaded by the plugin)
"""

import json
import os
import re
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import retranslate_clean as rc

PLUGIN = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAIN = os.path.join(PLUGIN, "translations", "dictionary_zh.json")
NAT = os.path.join(PLUGIN, "native", "translations", "dictionary_zh.json")
OUT = os.path.join(PLUGIN, "tools", "scratch", "third_party_internal.json")

# image-format / camera / code-internal markers beyond rc's list
EXTRA_MARKERS = [
    "PNG","JPEG","JPG","jpg","jpeg","png","IDAT","iCCP","tRNS","IHDR","bKGD",
    "sRGB","Chromatic","charset","scanline","scanlines","StripByte","Bayer",
    "Exif","EXIF","ICCProfile","ICC profile","ColorSpace","colorimetric",
    "Colorspace","CFAPlane","FocalPlane","ResolutionUnit","BitsPer","Photometric",
    "YCbCr","Adobe","Photoshop","JPEGInterchange","Quantization","Quantiz",
    "Huffman","Deflate","zlib","deflate","inflate","Codestream","TileOff",
    "Phase One","Aptus","PowerShot","San Francisco","Galaxy","Hasselblad","Canon",
    "Nikon","Olympus","Panasonic","Pentax","Casio","Sony","Fuji","Leica","Kodak",
    "Ricoh","Sigma","pxr","pxrInternal","pxrReserved","cdecl","pCAL","GetProxy",
    "WebP","WebPPicture","xmlns","Sdf","Pcp","LayerElement","Prim index",
    "Universal Scene","Alembic","Usd","USD","DigiCert","X509","certificate",
    "IPSec","roam file","Activation key","Activation database","Activation server",
    "licf","keyf","sCAL","bootstrap","chunk","ImageInfo","codec","bit_depth",
    "Sampling factors","sampleCount","ArResolver","ArResolved","asset resolver",
    "Deferring","variants","Unifi","Unity","Licens"
]

def is_non_speedtree(k):
    if rc.noise_key(k) or rc.is_tp(k) or rc.CODE_IDENT.search(k):
        return True
    kk = k.lower()
    for m in EXTRA_MARKERS:
        if m.lower() in kk:
            return True
    return False

def main():
    d = json.load(open(MAIN, encoding="utf-8"))["translations"]
    speed = {}
    non = {}
    for k, v in d.items():
        if is_non_speedtree(k):
            non[k] = v
        else:
            speed[k] = v
    print("total:", len(d), " speedtree:", len(speed), " non-speedtree:", len(non))
    sp = {"$schema":"sp-translation-v1","id":"speedtree-merged","language":"zh-CN",
          "description":"SpeedTree 中文汉化词库（SpeedTree 界面词条）","translations": speed}
    np = {"$schema":"sp-translation-v1","id":"speedtree-thirdparty","language":"zh-CN",
          "description":"非 SpeedTree 串（第三方/相机/图像格式/代码内部，不参与界面汉化）",
          "translations": non}
    for p in (MAIN, NAT):
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(sp, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(np, fh, ensure_ascii=False, indent=1)
        fh.write("\n")
    print("wrote main:", len(speed), " / non-speedtree ->", OUT, len(non))

if __name__ == "__main__":
    main()
