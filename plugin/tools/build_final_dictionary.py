#!/usr/bin/env python3
"""Merge freshly-translated SpeedTree strings into a shipping dictionary.

Adds dictionary_complete_zh.json containing only entries that are net-new
(present in the translated pool but not already non-empty in the curated
translations).  Written to both plugin/translations (source) and
native/translations (used by the injected DLL at runtime).  Later sorted
dictionaries still override on collisions, so curated entries win.
"""

import glob
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(HERE)
SRC_TRANS = os.path.join(PLUGIN, "translations")
NAT_TRANS = os.path.join(PLUGIN, "native", "translations")


def load_curated():
    """Filled translations from the 8 curated dictionaries (minus the
    auto-generated complete file, which is merged separately)."""
    filled = {}
    for p in glob.glob(os.path.join(SRC_TRANS, "*_zh.json")):
        if os.path.basename(p) == "dictionary_complete_zh.json":
            continue
        try:
            d = json.load(open(p, encoding="utf-8-sig"))
        except Exception:
            continue
        for k, v in d.get("translations", {}).items():
            if v.strip() and v != "_skip_":
                filled[k] = v
    return filled


def main():
    curated = load_curated()
    complete_path = os.path.join(SRC_TRANS, "dictionary_complete_zh.json")
    complete = {}
    if os.path.isfile(complete_path):
        try:
            d = json.load(open(complete_path, encoding="utf-8-sig"))
            complete = {k: v for k, v in d.get("translations", {}).items()
                        if v.strip() and v != "_skip_"}
        except Exception:
            pass

    trans = json.load(open(os.path.join(HERE, "scratch", "translated.json"), encoding="utf-8"))
    for k, v in trans.items():
        v = v.strip()
        if not v or v == k:          # skip placeholders / untouched noise
            continue
        if k in curated or k in complete:
            continue
        complete[k] = v

    payload = {
        "$schema": "sp-translation-v1",
        "id": "speedtree-complete",
        "language": "zh-CN",
        "description": "静态提取补全的 SpeedTree UI 与节点参数词条（自动翻译）",
        "translations": complete,
    }
    for outdir in (SRC_TRANS, NAT_TRANS):
        os.makedirs(outdir, exist_ok=True)
        out = os.path.join(outdir, "dictionary_complete_zh.json")
        with open(out, "w", encoding="utf-8") as fh:
            json.dump(payload, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
        print("wrote", out, "entries", len(complete))

    total = len(curated) + len(complete)
    print("curated:", len(curated), " complete:", len(complete),
          " new total:", total)


if __name__ == "__main__":
    main()
