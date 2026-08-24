#!/usr/bin/env python3
"""Final pass: move remaining embedded-library / EXIF / XML / system error strings
out of the main dictionary (into third_party_internal.json), keeping SpeedTree
sample asset/material names and real UI (to be hand-translated).
"""

import json
import os
import re

PLUGIN = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAIN = os.path.join(PLUGIN, "translations", "dictionary_zh.json")
NAT = os.path.join(PLUGIN, "native", "translations", "dictionary_zh.json")
OUT = os.path.join(PLUGIN, "tools", "scratch", "third_party_internal.json")

# embedded error / sentence language: declarative errors, XML/EXIF/system
ERR = re.compile(
    r"(\bis\b|\bare\b|\bwas\b|\bwere\b|\bhas\b|\bhave\b|\bdoes\b|n't\b|\bmust\b|"
    r"\bnot\b|\bcannot\b|\buna?ble\b|Illegal|Invalid|Incorrect|Improper|Incompatible|"
    r"Inserted|Index |File |Key |Keys |Buffer |Memory |Out of|Too |Error|Failed|"
    r"Warning|Exception|Declaration|Encryption|Codec|Encoding|System Clock|"
    r"deadlock|Google|web services|OpenGL|WinAPI|Windows|Thread|Socket|Network|"
    r"Times?amples|Kilometer|Meter|Mile|Inch|Foot|Hour|Minute|Second|"
    r"Microsoft|Resource |Result |Function |Generator pass|Geometry Force|"
    r"Messages for|Select buddy|Samples per|TimeSamples|Requested|Retrieving|"
    r"signature|boundary|overflow|underflow|syntax|parse|schema|xml|dtd|"
    r"property type|property count|not suitable|not supported|not implemented|"
    r"not found|not valid|not enough|not defined|does not|cannot be|must be|"
    r"should be|will be|has been|have been|was not|are not|is not|it |the |"
    r"and |or |of |to |for |with |a |in the|on the|at the|by the|"
    r"this |that |these |those |from |into |about |after |before |"
    r"\"[^\"]*\")", re.I)

def is_error(k):
    # short non-sentence asset names (with material terms) are keepable
    if re.match(r"^(BOb|BigBranches|BigRoots|BigVine)$", k):
        return False
    if re.search(r"Error|ERR|exception", k, re.I):
        return True
    # explicit embedded-lib/EXIF/XML/system markers (case-insensitive substring)
    for mk in ["Library","Lic ","Line length","Load all","Logged","Login","MallocTags",
               "Max Aperture","Min Aperture","Memory","Mode Message","Modified","Motion Base",
               "My Scene","Name expected","Named","New client","Noise (late)","Noise late",
               "Non-square","Nothing defined","Null map","Object Type Reference","Obtain Cache",
               "Out-of-range","Ov Size","Parent curve count","Perceptual preserving","Pixel composition",
               "Plug In","Policy","Populate","Portable","Preview Image","Print Image","PrintIM",
               "Problematic stitch","Producer","Image resolution","Letterboxed","Level coordinate",
               "Level ofI","Logged","Knock out others","Leaf AFi","Image resolution","Max entity",
               "Software","Some ","Sorry","Spatial","Spectral","Speed of","Spheric","Squared",
               "Standard atm","Start Of","Step must","Stem Main","Sticky","Stately","State mismatch",
               "Space required","Space needed","TimeSamples","Timeout","Too much","Too many",
               "Tools support","Token","Transfer func","Translated binaries","Translator",
               "Up Axis","Unique image","Unregistered","Unresolved","User comments","Valid palette",
               "Width overflow","Wind level offset","Whole map","Valid image","Value clips",]:
        if mk.lower() in k.lower():
            return True
    for mk in ["Load no loadab","Maximum entity","Other Precision","Pipeline","Pn Size",
               "Print scene graph","QHash","QName","Rasterized Caption","Raw Image",
               "Read out-of-bounds","Reference Date","Reference Number","Rehostable",
               "Reinhard","Related Image","Relax-NG","Relocate already","Resampling Rate",
               "Roam OK","RotationUnits","SEASONS","Saturation preserving","Scan line",
               "Scope ","Server","Setting layer","Short Tons","Similarity Index","Sink edges",
               "Skylake","Square feet","Square yards","Subject Reference","Subject area",
               "Subject distance","Subject location","Sun Raster","Time Created","Time Mode",
               "Time Stamping","Time Zone","Time elapsed","Trace global","TraceRegistry",
               "WBBlue","WBGreen","WBRed","Write Array","Wrong","Zero strips","Zoom allH",
               "all frames","Amount","Paste ","Tags","Trim Mode","Treat ","Tone ","Theme "]:
        if mk.lower() in k.lower():
            return True
    if k.count(" ") >= 3 and (re.search(r"[.!]", k) or ERR.search(k)):
        return True
    return ERR.search(k) is not None

def main():
    d = json.load(open(MAIN, encoding="utf-8"))["translations"]
    keep, drop = {}, {}
    for k, v in d.items():
        if is_error(k):
            drop[k] = v
        else:
            keep[k] = v
    print("total:", len(d), " keep:", len(keep), " drop(embedded):", len(drop))
    sp = {"$schema":"sp-translation-v1","id":"speedtree-merged","language":"zh-CN",
          "description":"SpeedTree 中文汉化词库（SpeedTree 界面词条）","translations": keep}
    np = {"$schema":"sp-translation-v1","id":"speedtree-thirdparty","language":"zh-CN",
          "description":"非 SpeedTree 串（嵌入库报错/EXIF/XMl/系统），不参与界面汉化",
          "translations": drop}
    for p in (MAIN, NAT):
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(sp, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(np, fh, ensure_ascii=False, indent=1)
        fh.write("\n")
    print("wrote main:", len(keep), " / third-party:", len(drop))
    # show what's kept (asset/material names to translate)
    kept = sorted(keep)
    print("sample kept (to hand-translate):", kept[:40])

if __name__ == "__main__":
    main()
