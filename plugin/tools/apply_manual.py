#!/usr/bin/env python3
"""Merge hand-authored translations (manual_zh.MANUAL) into dictionary_zh.json,
replacing machine-composed values for those keys.  Keeps everything else.
"""

import json
import glob
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import manual_zh

PLUGIN = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAIN = os.path.join(PLUGIN, "translations", "dictionary_zh.json")
NAT = os.path.join(PLUGIN, "native", "translations", "dictionary_zh.json")

def main():
    d = json.load(open(MAIN, encoding="utf-8"))["translations"]
    n = 0
    for k, v in manual_zh.MANUAL.items():
        d[k] = v
        n += 1
    # merge per-batch hand-authored JSON files (manual/manual_batch_*.json)
    manual_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "manual")
    for p in sorted(glob.glob(os.path.join(manual_dir, "manual_batch_*.json"))):
        b = json.load(open(p, encoding="utf-8"))
        for k, v in b.items():
            d[k] = v
            n += 1
    # Confirmed binary-symbol noise; never promote it into the runtime dictionary.
    d.pop("BOb", None)
    payload = {"$schema":"sp-translation-v1","id":"speedtree-merged","language":"zh-CN",
               "description":"SpeedTree 中文汉化词库（人工精翻版）","translations": d}
    for p in (MAIN, NAT):
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(payload, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
    print("applied", n, "hand-authored entries ->", len(d), "total")

if __name__ == "__main__":
    main()
