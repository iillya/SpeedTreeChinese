#!/usr/bin/env python3
"""Build an sp-translation-v1 skeleton dictionary from a captured text inventory.

Reads a newline-delimited English-string file (e.g. probe\\out\\all_texts.txt),
filters out non-translatable strings (numbers, URLs, HTML, already-Chinese), and
merges against any existing *_zh.json so already-filled translations survive.

Empty translations are placeholders: the engine ignores them, so filling values
in this file is how you add coverage without shipping translation for every term.
"""

import argparse
import json
import os
import re
import sys

PLUGIN = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TRANSLATIONS = os.path.join(PLUGIN, "translations")

NUMERIC = re.compile(r"^[+-]?(?:\d+(?:[.,]\d+)?|[.,]\d+)(?:[eE][+-]?\d+)?%?$")
ASSET_URL = re.compile(r"\?version=|^/")
HTML = re.compile(r"<[^>]+>")
CJK = re.compile(r"[\u3400-\u9FFF\uF900-\uFAFF]")


def is_translatable(s: str) -> bool:
    t = s.strip()
    if not t:
        return False
    if CJK.search(t):
        return False
    if NUMERIC.match(t):
        return False
    if ASSET_URL.search(t):
        return False
    if HTML.search(t):
        return False
    # Values like "Branches 3" / "0-1 patches" are mostly UI state, not labels.
    if len(t) < 2:
        return False
    # Skip pure whitespace control markers / separators.
    if t in ("-", "--", "", "|"):
        return False
    return True


def load_existing():
    existing = {}
    if not os.path.isdir(TRANSLATIONS):
        return existing
    for name in sorted(os.listdir(TRANSLATIONS)):
        if not name.endswith("_zh.json"):
            continue
        path = os.path.join(TRANSLATIONS, name)
        try:
            with open(path, encoding="utf-8-sig") as fh:
                data = json.load(fh)
            existing.update(data.get("translations", {}))
        except Exception:
            pass
    return existing


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", nargs="?", default=os.path.join(
        os.path.dirname(PLUGIN), "probe", "out", "all_texts.txt"))
    ap.add_argument("-o", default=os.path.join(TRANSLATIONS, "dictionary_pending_zh.json"))
    args = ap.parse_args()

    if not os.path.isfile(args.input):
        raise SystemExit(f"inventory not found: {args.input}")
    with open(args.input, encoding="utf-8") as fh:
        lines = fh.read().splitlines()

    existing = load_existing()
    translations = {}
    for line in lines:
        key = line.rstrip()
        if not is_translatable(key):
            continue
        if key in existing:
            translations[key] = existing[key]
        else:
            translations[key] = ""  # placeholder

    payload = {
        "$schema": "sp-translation-v1",
        "id": "speedtree-pending",
        "language": "zh-CN",
        "description": "待填写的 SpeedTree 词条（空条目为占位符，不被引擎加载）",
        "translations": translations,
    }
    os.makedirs(os.path.dirname(args.o), exist_ok=True)
    with open(args.o, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, ensure_ascii=False, indent=2)
        fh.write("\n")
    total = len(translations)
    filled = sum(1 for v in translations.values() if v)
    print(f"wrote {args.o}: {total} entries ({filled} already filled)")


if __name__ == "__main__":
    main()
