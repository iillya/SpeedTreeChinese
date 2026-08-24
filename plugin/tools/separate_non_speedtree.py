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
    "Deferring","variants","Unifi","Unity","Licens",
    "Adaptive Optics","Albedo With","AttValue","Attribute element","Attribute default",
    "Attribute index","Attribute time","Axis system","BeginWrite","Bearing of",
    "Blue box","Body serial","Bounce on","Broken pipe","Buffer overflow","Builder Two",
    "Bytes per","Client rejected","Comment not","Comment too","Connection refused",
    "Constant Key","Content Location","Country Name","Create Singleton","Crop Hi",
    "Cubic meters","Customer name","Cycle detected","Data Rate","DataType",
    "Date Created","DateTime","Debris","Debug XML","Default reached","Demo activation",
    "Deprecated","Detected an entity","Device settings","Did Change","Did not find",
    "Diffie","Digital Creation","Digital Zoom","Directory not empty","Document ",
    "Double hyphen","Draft (active)","ComputeInstance","Computed tile","Consult light",
    "Contact information","Corrupted","country","Diffie"
]

def is_non_speedtree(k):
    if rc.noise_key(k) or rc.is_tp(k) or rc.CODE_IDENT.search(k):
        return True
    kk = k.lower()
    for m in EXTRA_MARKERS:
        if m.lower() in kk:
            return True
    # sample asset / material names: have a digit or underscore suffix
    if re.search(r"[\d_]", k):
        return True
    # embedded error / status sentences & fragments
    if re.match(
        r"^(Bad|Can't|Cannot|Could|could|Application|An |The |This |A |If |when |"
        r"There |It |You |Please |No such|Unable|Unhandled|Unsupported|Unexpected|"
        r"Invalid|Insufficient|Missing|Failed|Malformed|Bogus|Premature|"
        r"Requested|Attempt|Attempted|Warning|Error|Exception|Not |Doesn't|Has |"
        r"Is |Are |Will |Would |Should |Must |May |Might |In order|Provided|"
        r"Found |Tried |Ignoring|Skipping|Compiling|Building|Generating|"
        r"Couldn't|Cannot|No |Unable|Requires|Require|The following|There was)", k):
        return True
    # short non-label tokens / bare operators
    if not re.search(r"[A-Za-z]{3,}", k):
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
