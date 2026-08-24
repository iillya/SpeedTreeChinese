#!/usr/bin/env python3
"""
Capture untranslated (QObject::tr) UI entries from a running SpeedTree Modeler.

The hook DLL ships an opt-in tr() probe (ST_TRPROBE=1): it patches
QCoreApplication::translate and records every tr(context, source) the app makes,
tagging whether the source is already in the loaded dictionary
(build/translations\\tr_probe.json -> per-entry "in_dict").  This script drives
that probe and turns the "in_dict == false" entries into a fillable dictionary.

Usage (from the repo root):
    python scripts/capture_untranslated.py                # capture, wait 90s
    python scripts/capture_untranslated.py --tour         # also auto-open panels
    python scripts/capture_untranslated.py --wait 180     # wait longer
    python scripts/capture_untranslated.py --until-close  # wait until app closes

Output:
    translations\\untranslated_captured_zh.json  (sp-translation-v1, empty values)
"""

import argparse
import glob
import json
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LAUNCHER = os.path.join(ROOT, "build", "SpeedTreeChineseLauncher.exe")
PROBE = os.path.join(ROOT, "build", "translations", "tr_probe.json")
OUT = os.path.join(ROOT, "translations", "untranslated_captured_zh.json")


def load_dictionary():
    """Merge every *_zh.json under build|repo translations (skip probes/tours)."""
    found = {}
    for base in (os.path.join(ROOT, "build", "translations"),
                 os.path.join(ROOT, "translations")):
        for path in glob.glob(os.path.join(base, "*_zh.json")):
            name = os.path.basename(path)
            if name in ("tr_probe_missing_zh.json", "untranslated_captured_zh.json"):
                continue
            try:
                with open(path, encoding="utf-8") as fh:
                    data = json.load(fh)
            except Exception:
                continue
            for k, v in (data.get("translations", {}) or {}).items():
                if isinstance(v, str) and v and "||" not in k and k not in found:
                    found[k] = v
    return found


def read_probe():
    if not os.path.exists(PROBE):
        return None
    with open(PROBE, encoding="utf-8") as fh:
        return json.load(fh)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tour", action="store_true", help="also set ST_TOUR=1 (auto-open panels)")
    ap.add_argument("--wait", type=int, default=90, help="seconds to let the app run before reading (default 90)")
    ap.add_argument("--until-close", action="store_true", help="wait until SpeedTree_Modeler exits instead of a timeout")
    ap.add_argument("--output", default=OUT, help="output dictionary file")
    args = ap.parse_args()

    if not os.path.isfile(LAUNCHER):
        sys.exit("launcher not found: " + LAUNCHER + "  (run source\\build.bat first)")

    env = dict(os.environ)
    env["ST_TRPROBE"] = "1"
    if args.tour:
        env["ST_TOUR"] = "1"

    print(">> launching", LAUNCHER, "(ST_TRPROBE=1%s)" % (" + ST_TOUR=1" if args.tour else ""))
    proc = subprocess.Popen([LAUNCHER], env=env)
    print(">> launcher pid", proc.pid)

    if args.until_close:
        print(">> waiting until SpeedTree_Modeler exits (close the app yourself)...")
        while True:
            # The launcher exits right after injecting; wait for the actual
            # SpeedTree_Modeler.exe process to be gone instead.
            out = subprocess.run(
                ["tasklist", "/FI", "IMAGENAME eq SpeedTree_Modeler.exe", "/FO", "CSV", "/NH"],
                capture_output=True, text=True).stdout
            if "SpeedTree_Modeler.exe" not in out:
                break
            time.sleep(1)
    else:
        print(f">> letting the app run {args.wait}s (operate panels to broaden coverage)...")
        time.sleep(args.wait)
        # give the 3s probe timer a beat to flush
        time.sleep(3)

    probe = read_probe()
    if probe is None:
        sys.exit("no tr_probe.json found -- SpeedTree may not have started / probe not active")

    entries = probe.get("translations", {})
    if not isinstance(entries, dict) or not entries:
        sys.exit("tr_probe.json has no entries -- nothing captured")

    # in_dict is computed against the dictionary at run time; also re-check ours.
    ddict = load_dictionary()
    missing, covered = {}, 0
    for key, val in entries.items():
        src = val.get("source", key) if isinstance(val, dict) else key
        if not src or src in ddict or val.get("in_dict"):
            covered += 1
            continue
        missing[src] = ""   # dictionary format: empty value -> fill in translation

    # Stable ordering by first-seen context/source for a predictable diff.
    ordered = {}
    for key in entries:
        src = entries[key].get("source", key) if isinstance(entries[key], dict) else key
        if src in missing and src not in ordered:
            ordered[src] = ""

    out = {
        "$schema": "sp-translation-v1",
        "id": "speedtree-captured",
        "language": "zh-CN",
        "description": "QObject::tr() 运行期采集的未翻译词条（待填写）",
        "translations": ordered,
    }
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as fh:
        json.dump(out, fh, ensure_ascii=False, indent=2)

    print("\n=== summary ===")
    print("tr() sources captured :", len(entries))
    print("already translated    :", covered)
    print("untranslated          :", len(ordered))
    print("wrote: " + args.output)
    if ordered:
        byctx = {}
        for key in entries:
            if isinstance(entries[key], dict) and entries[key].get("source") in ordered:
                byctx.setdefault(entries[key].get("context", "?"), 0)
                byctx[entries[key].get("context", "?")] += 1
        top = sorted(byctx.items(), key=lambda kv: -kv[1])[:15]
        print("top contexts:")
        for ctx, n in top:
            print(f"  {n:4d}  {ctx}")


if __name__ == "__main__":
    main()
