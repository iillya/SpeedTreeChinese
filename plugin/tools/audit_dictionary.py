#!/usr/bin/env python3
"""Full, non-sampling audit of the consolidated dictionary.

Classifies EVERY entry into: clean / leftover (incomplete translation) /
noise_key / suspect.  Writes scratch/audit_report.txt for line-by-line review.
"""

import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(HERE)
MAIN = os.path.join(PLUGIN, "translations", "dictionary_zh.json")

CJK = re.compile(r"[\u3400-\u9FFF\uF900-\uFAFF]")
ACR = {"LOD","UV","XYZ","IK","SDK","RGB","FPS","FOV","HDRI","TGA","PNG","JPG",
       "MIP","GPU","CPU","PVS","CRC","DDS","FBX","AO","GI","SSS","BMP","TIF",
       "U","V","W","X","Y","Z","SD","HD","AI","ID","I/O","NAT","AP","SLR"}

def latin_words(s):
    return [w for w in re.split(r"[^A-Za-z]+", s) if len(w) >= 2]

def leftover_english(v):
    words = latin_words(v)
    return [w for w in words if w not in ACR]

def noise_key(k):
    t = k.strip()
    if not (1 <= len(t) <= 80): return True
    if re.search(r"[\x00-\x1f]", t): return True
    if re.search(r"\\|/|::|0x|\.dll|\.exe|\.spm|\.png|\.tga", t): return True
    if re.search(r"[#{};=%<>|^~`$*+@\[\]]", t): return True
    # junk tokens: leading digit/symbol, single-letter pairs, all-caps+digit
    if re.match(r"^[-\d(]", t): return True
    if re.match(r"^[A-Z]\s?[A-Z]$", t): return True          # "U A" / "A B"
    if re.fullmatch(r"[A-Z0-9_\-]{2,}", t) and re.search(r"\d", t): return True
    return False

def main():
    d = json.load(open(MAIN, encoding="utf-8"))["translations"]
    clean, leftover, noise, suspect = [], [], [], []
    for k, v in d.items():
        if noise_key(k):
            noise.append((k, v)); continue
        le = leftover_english(v)
        if le:
            leftover.append((k, v)); continue
        if not CJK.search(v):
            suspect.append((k, v)); continue
        clean.append((k, v))
    report = os.path.join(HERE, "scratch", "audit_report.txt")
    os.makedirs(os.path.dirname(report), exist_ok=True)
    with open(report, "w", encoding="utf-8") as fh:
        def w(title, arr):
            fh.write("\n===== %s (%d) =====\n" % (title, len(arr)))
            for k, v in arr:
                fh.write("%s\t=>\t%s\n" % (k, v))
        w("CLEAN", clean)
        w("LEFTOVER_ENG", leftover)
        w("NOISE_KEY", noise)
        w("SUSPECT", suspect)
    print("TOTAL", len(d))
    print("clean:", len(clean), " leftover:", len(leftover),
          " noise_key:", len(noise), " suspect:", len(suspect))
    print("report:", report)

if __name__ == "__main__":
    main()
