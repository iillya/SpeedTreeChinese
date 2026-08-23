#!/usr/bin/env python3
"""Merge every *_zh.json in plugin/translations into a single self-contained
dictionary_zh.json and remove the split files (both source and native copies).

Merge rule matches the engine: empty values are ignored, for a repeated key the
last NON-EMPTY value (in filename order) wins.
"""

import glob
import json
import os
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(HERE)
SRC = os.path.join(PLUGIN, "translations")
NAT = os.path.join(PLUGIN, "native", "translations")


def merged_dict(dirpath):
    merged = {}
    for p in sorted(glob.glob(os.path.join(dirpath, "*_zh.json"))):
        try:
            d = json.load(open(p, encoding="utf-8"))
        except Exception:
            continue
        for k, v in d.get("translations", {}).items():
            if v.strip() and v != "_skip_":
                merged[k] = v
    return merged


def main():
    merged = merged_dict(SRC)
    payload = {
        "$schema": "sp-translation-v1",
        "id": "speedtree-merged",
        "language": "zh-CN",
        "description": "SpeedTree 中文汉化词库（合并版，自包含）",
        "translations": merged,
    }
    main_file = os.path.join(SRC, "dictionary_zh.json")
    with open(main_file, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, ensure_ascii=False, indent=1)
        fh.write("\n")
    print("merged", len(merged), "entries -> dictionary_zh.json")

    # remove the split files (keep dictionary_zh.json) in both locations
    for d in (SRC, NAT):
        for p in glob.glob(os.path.join(d, "*_zh.json")):
            if os.path.basename(p) == "dictionary_zh.json":
                continue
            try:
                os.remove(p)
            except Exception as e:
                print("remove failed", p, e)
    os.makedirs(NAT, exist_ok=True)
    shutil.copyfile(main_file, os.path.join(NAT, "dictionary_zh.json"))
    print("native dictionary_zh.json synced")


if __name__ == "__main__":
    main()
