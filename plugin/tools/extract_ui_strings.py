#!/usr/bin/env python3
"""Statically extract candidate English UI strings from the SpeedTree install.

Runtime sniffing (F12) only sees controls that exist at that moment. Static
extraction reads the installed SpeedTree_Modeler.exe (string literals live in
the PE .rdata/.data sections) and reuses the runtime inventory, then writes an
sp-translation-v1 skeleton. Empty values are placeholders (ignored by the
engine until filled).

Outputs:
    translations/static_zh.json          sp-translation-v1 skeleton
    translations/static_ui_candidates.txt  plain text review list
"""

import argparse
import json
import os
import re
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TRANSLATIONS = os.path.join(ROOT, "translations")

NUMERIC = re.compile(r"^[+-]?(?:\d+(?:[.,]\d+)?|[.,]\d+)(?:[eE][+-]?\d+)?%?$")
PATHY = re.compile(r"[\\/]|[.]\w{1,4}\b")
URLY = re.compile(r"http|www|\.com\b|\.org\b|@|github|speedtree\.com", re.IGNORECASE)
ALL_CAPS = re.compile(r"^[A-Z0-9][A-Z0-9_ ]*$")
HEXL = re.compile(r"^[0-9a-fA-F]{6,}$")
CJK = re.compile(r"[\u3400-\u9FFF\uF900-\uFAFF]")
CODE = re.compile(r"(std::|Q[A-Z]|::|Qt6|nullptr|malloc|_ZN|\.obj\b|##|0x)")
SINGLE_LABEL = re.compile(r"^[A-Z][a-z]{1,}$")        # "Mesh", "Length"
PARAM_SUFFIX = re.compile(
    r"(Length|Radius|Seed|Count|Scale|Variance|Amount|Offset|Gravity|Force|"
    r"Color|Size|Width|Height|Angle|Weight|Density|Jitter|Rotation|Roll|Curl|"
    r"Fold|Speed|Distribution|Noise|Strength|Thickness|Curve|Profile|Bend|"
    r"Spread|Taper|Align|Pattern|Shape)$")


def split_camel(token):
    """BranchLength -> Branch Length; selfLeftRight -> self Left Right."""
    out = []
    for i, ch in enumerate(token):
        if i and ch.isupper() and token[i - 1].islower():
            out.append(" ")
        out.append(ch)
    return "".join(out).strip()


def extract_camelcase(buf, min_len=6):
    run = bytearray()
    for b in buf:
        if 0x41 <= b <= 0x5A or 0x61 <= b <= 0x7A:
            run.append(b)
        else:
            if len(run) >= min_len:
                yield bytes(run).decode("ascii", "replace")
            run = bytearray()
    if len(run) >= min_len:
        yield bytes(run).decode("ascii", "replace")


def sections(data):
    if len(data) < 0x40:
        return []
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        return []
    nsec = struct.unpack_from("<H", data, e_lfanew + 6)[0]
    opt_size = struct.unpack_from("<H", data, e_lfanew + 20)[0]
    table = e_lfanew + 24 + opt_size
    out = []
    for i in range(nsec):
        off = table + i * 40
        name = data[off:off + 8].rstrip(b"\x00").decode("ascii", "replace")
        vsize = struct.unpack_from("<I", data, off + 8)[0]
        raw_size = struct.unpack_from("<I", data, off + 16)[0]
        raw_ptr = struct.unpack_from("<I", data, off + 20)[0]
        if name and raw_ptr and raw_size and raw_size < len(data):
            out.append((name, data[raw_ptr:raw_ptr + min(raw_size, vsize or raw_size)]))
    return out


def extract_ascii(buf, min_len=3):
    run = bytearray()
    for b in buf:
        if 0x20 <= b <= 0x7E:
            run.append(b)
        else:
            if len(run) >= min_len:
                yield bytes(run)
            run = bytearray()
    if len(run) >= min_len:
        yield bytes(run)


def extract_utf16le(buf, min_len=3):
    run = bytearray()
    i = 0
    n = len(buf)
    while i + 1 < n:
        lo, hi = buf[i], buf[i + 1]
        if hi == 0 and 0x20 <= lo <= 0x7E:
            run.append(lo)
            i += 2
        else:
            if len(run) >= min_len:
                yield bytes(run)
            run = bytearray()
            i += 1
    if len(run) >= min_len:
        yield bytes(run)


CODE_PUNCT = set("#{};=%\\<>|^~`")


def keep(text, strict=True):
    t = text.strip()
    if not (2 <= len(t) <= 160):
        return False
    if CJK.search(t):
        return False
    if NUMERIC.match(t):
        return False
    if PATHY.search(t):
        return False
    if URLY.search(t):
        return False
    if ALL_CAPS.match(t) and " " in t:
        return False
    if HEXL.match(t):
        return False
    if CODE.search(t):
        return False
    if not re.search(r"[A-Za-z]", t):
        return False
    if strict:
        if any(c in CODE_PUNCT for c in t):
            return False
        if "_" in t:
            return False
        if "(" in t or ")" in t or ":" in t:
            return False
        if len(t) > 80:
            return False
        # UI labels: start with an uppercase letter (Title case / single word).
        if not t[0].isalpha() or not t[0].isupper():
            return False
        if re.search(r"\b(Cannot|Removed|display this|No matching|Roaming|hostid|license|Licensing|ISA|MXCSR|Threads|Features)\b", t):
            return False
        if re.search(r"\b(The|and|in the|for all|This can be|this can be)\b", t):
            return False
    # Letter+space weight must be dominant (avoids hex-opcode-ish noise).
    letters = sum(1 for c in t if c.isalpha() or c == " " or c in ".,:;'\"-")
    if letters / len(t) < 0.75:
        return False
    return True


def is_label(seg):
    t = seg.strip()
    if not (1 <= len(t) <= 80):
        return False
    if not t[0].isalpha():
        return False
    if re.search(r"[#{};=%\\<>|^~`/\\:.]", t):
        return False
    if not re.search(r"[A-Za-z]", t):
        return False
    if re.match(r"^[A-Z0-9_]{2,}$", t) and not re.search(r"\s", t):
        return False
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--install",
                    default=r"C:\Program Files\SpeedTree\SpeedTree Modeler v10.2.0\win64")
    ap.add_argument("-o", default=os.path.join(TRANSLATIONS, "static_zh.json"))
    args = ap.parse_args()

    exe = os.path.join(args.install, "SpeedTree_Modeler.exe")
    if not os.path.isfile(exe):
        raise SystemExit(f"exe not found: {exe}")
    with open(exe, "rb") as fh:
        data = fh.read()

    candidates = set()
    # Scan the WHOLE image (all sections) so inline string literals in .text
    # (which many SpeedTree node parameters live in) are not missed.
    for buf in [data]:
        for run in extract_ascii(buf):
            s = run.decode("ascii", "replace")
            # Node parameters are packed as "Label:Label:Label" lists in .text.
            if ":" in s and s.count(":") >= 1:
                for seg in s.split(":"):
                    seg = seg.strip()
                    if seg and is_label(seg):
                        candidates.add(seg)
            elif keep(s):
                candidates.add(s)
        for run in extract_utf16le(buf):
            s = run.decode("ascii", "replace")
            if keep(s):
                candidates.add(s)
        # Node parameters: PascalCase property IDs (BranchLength, SectionVariance…)
        # are turned into spaced UI labels.
        for tok in extract_camelcase(buf):
            if PARAM_SUFFIX.search(tok):
                derived = split_camel(tok)
                if keep(derived):
                    candidates.add(derived)

    # Read runtime inventory (authoritative UI strings already seen in-app).
    runtime = os.path.join(os.path.dirname(ROOT), "probe", "out", "all_texts.txt")
    runtime_set = set()
    runtime_sources = [
        runtime,
        os.path.join(ROOT, "native", "drawtext_captured.txt"),
    ]
    for src in runtime_sources:
        if not os.path.isfile(src):
            continue
        with open(src, encoding="utf-8") as fh:
            for line in fh:
                r = line.rstrip()
                if keep(r, strict=False):
                    runtime_set.add(r)

    # Fusion: keep everything seen at runtime; keep static multi-word phrases
    # (menu items / tooltips / dialogs); drop static single-token identifiers.
    final = set(runtime_set)
    for c in candidates:
        if c in runtime_set:
            final.add(c)
        elif " " in c:
            final.add(c)
        elif SINGLE_LABEL.match(c):
            final.add(c)
        else:
            continue

    # Normalize keys: strip a leading '&' mnemonic, dedupe.
    normalized = set()
    for k in final:
        kk = k.lstrip("&").strip()
        if kk:
            normalized.add(kk)

    ordered = sorted(normalized)
    payload = {
        "$schema": "sp-translation-v1",
        "id": "speedtree-static",
        "language": "zh-CN",
        "description": "静态提取的 SpeedTree UI 词条（空条目为占位符）",
        "translations": {k: "" for k in ordered},
    }
    os.makedirs(TRANSLATIONS, exist_ok=True)
    with open(args.o, "w", encoding="utf-8") as fh:
        json.dump(payload, fh, ensure_ascii=False, indent=1)
        fh.write("\n")
    lst = os.path.join(TRANSLATIONS, "static_ui_candidates.txt")
    with open(lst, "w", encoding="utf-8") as fh:
        fh.write("\n".join(ordered))
    print(f"extracted {len(ordered)} candidate strings from {exe}")
    print(f"  dictionary: {args.o}")
    print(f"  list:       {lst}")


if __name__ == "__main__":
    main()
