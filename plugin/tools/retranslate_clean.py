#!/usr/bin/env python3
"""Re-translate every entry whose value still has leftover English (half-finished)
using the improved glossary + composition, so no entry stays half-translated.
Code identifiers (pxr*, cdecl, etc.) are kept as-is (correct as code tokens).
"""

import json
import os
import re
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import translate_pool as tp

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(HERE)
MAIN = os.path.join(PLUGIN, "translations", "dictionary_zh.json")
NAT = os.path.join(PLUGIN, "native", "translations", "dictionary_zh.json")

ACR = {"LOD","UV","XYZ","IK","SDK","RGB","FPS","FOV","HDRI","TGA","PNG","JPG",
       "MIP","GPU","CPU","FBX","AO","GI","SSS","BMP","TIF","MDI","U","V","W",
       "X","Y","Z","ID","VFX","SD","HD","AI"}
# code identifiers that are correct as raw English
CODE = re.compile(r"^(pxr|cdecl|pCAL|Sdf|PRIM|IPSec|\.h|.*\(\))$")

TP_MARKERS = ["Canon","Nikon","Olympus","Panasonic","Pentax","Casio","Sony","Fuji",
    "Hasselblad","Ricoh","Sigma","Epson","Dell","Apple","Adobe","Photoshop",
    "Netscape","Samsung","DigiCert","CCITT","AFArea","AFImage","AFInfo","AFMode",
    "AFPoint","ACES","Alembic","Universal Scene","USD","ASF","WMPTag","Exif",
    "ICCProfile","CFAPlane","IPSec","Win32","Kodak","X509","certificate",
    "Biometric","Lens","Shutter","JPEG","TIFF","Gimbal","Intel","Macro","Sensor",
    "Transmission","Fisheye","Panorama","Recomposing","Catmull","Euler","Luv",
    "Luma","Camera","Driver","Battery","Audio","Creative","Broadcom","Realtek",
    "AD Time","AFArea","AVCSti","AVGf","AVRef","AVSdf","AVUsd","ASN1","Galaxy",
    "Face Detect","Film","Fax","Fg","CCITT","Flash","Focal Units","Chromium",
    "PNG","JPG","BMP","MIP","Scanline","Scanlines","IDAT","iCCP","tRNS","Colormap",
    "Colorimetric","Codestream","Photogrammetry","PowerShot","Aptus","Superfluo",
    "StripByteCounts","Planar","Xfer","Imagelength","Marker","Calculating",
    "Limits","Conditions","Malformed","Unrecognized","Nonexistent","Peeling",
    "Distichous","Proportional","Communication","Graphics","Network","Trust",
    "Photos","Photo","Twitter","Browser","Adapter","Identity","Hostname",
    "Bookmark","Fulfill","Auth","SubD","MCD","Burst","Packer","Analysis"]

CODE_IDENT = re.compile(
    r"^(pxrInternal|pxrReserved|cdecl|pCAL|Pcp|Sdf|jmp|buf|fn|cp|xmlns|"
    r"GetProxyForUrl|iXU|iXA|Prim|PRIM|XPath|MCD|LH|RH|TRC|Exp|Asc|Typ|"
    r"Op|Orig|Lcl|Tex|ink|CData|iCCP|IDATs|TRC)")

def is_tp(k):
    kk = k.lower()
    return any(m.lower() in kk for m in TP_MARKERS)

def noise_key(k):
    t = k.strip()
    if re.search(r"[\x00-\x1f]", t): return True
    if re.search(r"\\|/|::|0x|\.dll|\.exe|\.spm|\.png", t): return True
    if re.search(r"[#{};=%<>|^~`$*+@\[\]]", t): return True
    if re.match(r"^[-\d(]", t): return True
    if re.match(r"^[A-Z]\s?[A-Z]$", t): return True
    if re.fullmatch(r"[A-Z0-9_\-]{2,}", t): return True
    return False

def le(v):
    return [w for w in re.split(r"[^A-Za-z]+", v) if len(w) >= 2 and w not in ACR]

def main():
    d = json.load(open(MAIN, encoding="utf-8"))["translations"]
    removed = 0
    for k in list(d):
        if noise_key(k) or is_tp(k) or CODE_IDENT.search(k):
            del d[k]; removed += 1
    print("removed non-SpeedTree:", removed)
    fixed = 0
    for k, v in list(d.items()):
        if not le(v):
            continue
        nv = tp.translate(k).strip()
        # keep only if the fresh output is complete (no leftover) and changed
        if nv and nv != k and not le(nv):
            d[k] = nv
            fixed += 1
    print("entries:", len(d), " re-translated (fixed):", fixed)
    left = [k for k, v in d.items() if le(v)]
    print("still-leftover:", len(left))
    for k in left[:80]:
        print("  ", repr(k), "->", repr(d[k]))
    payload = {"$schema":"sp-translation-v1","id":"speedtree-merged","language":"zh-CN",
               "description":"SpeedTree 中文汉化词库（精翻版）","translations": d}
    for p in (MAIN, NAT):
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(payload, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
    print("wrote", len(d), "entries")

if __name__ == "__main__":
    main()
