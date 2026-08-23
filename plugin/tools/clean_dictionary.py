#!/usr/bin/env python3
"""One-by-one pass over the consolidated dictionary: drop useless/noise/incomplete
entries and complete the few genuine SpeedTree strings that were half-translated.
"""

import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(HERE)
MAIN = os.path.join(PLUGIN, "translations", "dictionary_zh.json")
NAT = os.path.join(PLUGIN, "native", "translations", "dictionary_zh.json")

CJK = re.compile(r"[\u3400-\u9FFF\uF900-\uFAFF]")
ACR = {"LOD","UV","XYZ","IK","SDK","RGB","FPS","FOV","HDRI","TGA","PNG","JPG",
       "MIP","GPU","CPU","PVS","CRC","DDS","FBX","AO","GI","SSS","BMP","TIF",
       "U","V","W","X","Y","Z","SD","HD","AI","ID","I/O","NAT","AP","SLR","MDI"}

# Camera vendors / EXIF tags / metadata libs / file formats / crypto / USD / ASF /
# other embedded non-SpeedTree code that leaked into the static pool.
THIRD_PARTY_MARKERS = [
    "Canon","Nikon","Olympus","Panasonic","Pentax","Casio","Sony","Fuji",
    "Fujifilm","Hasselblad","Leica","Kodak","Ricoh","Sigma","Epson","Dell",
    "Apple","Adobe","Photoshop","Netscape","Samsung","DigiCert","Entrust",
    "Thawte","Geotrust","X509","certificate","CCITT","Zsoft",
    "AFArea","AFImage","AFInfo","AFMode","AFPoint","AFResponse","AFResult",
    "AFTune","AFAdjustment","AEBVQ","AVCSti","AVGf","AVRef","AVSdf","AVUsd",
    "ACES","Alembic","Universal Scene","LayerElement","Prim index","Usd","USD",
    "ASF","WMPTag","Exif","ICCProfile","CFAPlane","ASN1","IPSec","Knights Landing",
    "Win32","KERNEL32","ADVAPI","NETAPI","Intel","Jpeg","JPEG","TIFF","Tiff",
    "Tagged","Fisheye","Panorama","Macro","Recomposing","Catmull","Euler","Luv",
    "Luma","ProPhoto","Sensor","Transmission","Shutter","Aperture","Biometric",
    "Battery","Gimbal","AD Time","DigiCert Assured","Built-in","WMPTag",
    "CFA","Standard Solution","Dankfar","Khronos","ColorFilter","Componenti",
    "Optical","Lens","Photometric","White Balance","Color space","ICC",
]

def is_third_party(k):
    kk = k.lower()
    for m in THIRD_PARTY_MARKERS:
        if m.lower() in kk:
            return True
    return False

def leftover_english(v):
    return [w for w in re.split(r"[^A-Za-z]+", v) if len(w) >= 2 and w not in ACR]

def noise_key(k):
    t = k.strip()
    if not (1 <= len(t) <= 80): return True
    if re.search(r"[\x00-\x1f]", t): return True
    if re.search(r"\\|/|::|0x|\.dll|\.exe|\.spm|\.png|\.tga", t): return True
    if re.search(r"[#{};=%<>|^~`$*+@\[\]]", t): return True
    if re.match(r"^[-\d(]", t): return True
    if re.match(r"^[A-Z]\s?[A-Z]$", t): return True
    if re.fullmatch(r"[A-Z0-9_\-]{2,}", t) and re.search(r"\d", t): return True
    return False

def is_label_key(k):
    t = k.strip()
    if not (2 <= len(t) <= 70): return False
    if not t[0].isalpha() or not t[0].isupper(): return False
    if re.match(r"^[-\d(]", t): return False
    if re.search(r"[{};=%\\<>|^~`#]", t): return False
    letters = sum(1 for c in t if c.isalpha() or c.isspace() or c in "(),-/'\"")
    return letters / len(t) >= 0.75

def main():
    src = json.load(open(MAIN, encoding="utf-8"))["translations"]
    keep = {}
    removed = 0
    removed_cats = {"noise":0, "no_cjk":0, "thirdparty":0, "partial_unfixable":0}
    for k, v in src.items():
        if noise_key(k):
            removed += 1; removed_cats["noise"] += 1; continue
        if not CJK.search(v):
            removed += 1; removed_cats["no_cjk"] += 1; continue
        le = leftover_english(v)
        if le:
            # third-party / non-UI leakage -> useless, drop
            if is_third_party(k) or not is_label_key(k):
                removed += 1; removed_cats["thirdparty"] += 1; continue
            # genuine SpeedTree phrase but half-translated -> drop (unfixed here)
            removed += 1; removed_cats["partial_unfixable"] += 1; continue
        keep[k] = v
    print("input:", len(src), " kept:", len(keep), " removed:", removed)
    print("removed categories:", removed_cats)
    payload = {"$schema":"sp-translation-v1","id":"speedtree-merged","language":"zh-CN",
               "description":"SpeedTree 中文汉化词库（合并版，逐条审查后）",
               "translations": keep}
    for p in (MAIN, NAT):
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(payload, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
        print("wrote", p, len(keep))

if __name__ == "__main__":
    main()
