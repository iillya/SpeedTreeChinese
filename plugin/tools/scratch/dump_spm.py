#!/usr/bin/env python3
import gzip, re, sys, glob, os

def names_in(path):
    raw = open(path, "rb").read()
    if raw[:2] == b"\x1f\x8b":
        txt = gzip.decompress(raw).decode("utf-8", "replace")
    else:
        txt = raw.decode("utf-8", "replace")
    return txt

st = r"C:\Program Files\SpeedTree\SpeedTree Modeler v10.2.0\samples\Games\Bush\Bush_Desktop.spm"
path = sys.argv[1] if len(sys.argv) > 1 else st
txt = names_in(path)
print("decompressed len:", len(txt))
print("head:", repr(txt[:200]))
print("has xml header:", txt.lstrip().startswith("<?xml"))
names = re.findall(r'name="([^"]+)"', txt)
print("name= attributes:", len(names))
uniq = sorted(set(names))
print("unique names:", len(uniq))
print("sample:", uniq[:80])
props = re.findall(r'<[Pp]roperty[^>]*>', txt)
print("property tags:", len(props))
if props:
    print("prop sample:", props[:10])

# Show the first few Property blocks (with a little surrounding text)
for m in re.finditer(r'<Property>.*?</Property>', txt, re.S):
    seg = m.group(0)
    if len(seg) < 900:
        print("=== Property block ===")
        print(seg[:700])
    cnt = getattr(sys, "_cnt", 0) + 1
    sys._cnt = cnt
    if cnt >= 5:
        break

# All property <Name> values (the full path Group:Subgroup:Label).
names = re.findall(r'<Name>([^<]+)</Name>', txt)
print("\nTOTAL <Name>:", len(names))
uniq = []
for n in names:
    if n not in uniq:
        uniq.append(n)
print("unique <Name>:", len(uniq))
print("first 120:")
for n in uniq[:120]:
    print("  ", repr(n))

# Across ALL sample .spm files: gather the last ':' segment (the UI label).
import glob
sample_root = r"C:\Program Files\SpeedTree\SpeedTree Modeler v10.2.0\samples"
labels = set()
paths = set()
for p in glob.glob(os.path.join(sample_root, "**", "*.spm"), recursive=True):
    try:
        t = gzip.decompress(open(p, "rb").read()).decode("utf-8", "replace")
    except Exception:
        continue
    for n in re.findall(r"<Name>([^<]+)</Name>", t):
        paths.add(n)
        labels.add(n.split(":")[-1].strip())
print("\n=== ALL SAMPLES ===")
print("unique full paths:", len(paths), " unique labels:", len(labels))
def is_label(l):
    t = l.strip()
    if not (2 <= len(t) <= 60): return False
    if not t[0].isalpha() or not t[0].isupper(): return False
    # leading digit / paren / value noise
    if re.match(r"^[-\d(]", t): return False
    if re.search(r"[{};=%\\<>|^~`#]", t): return False
    if re.search(r"\.\w{1,4}$", t): return False
    letters = sum(1 for c in t if c.isalpha() or c.isspace() or c in "(),-/'\"")
    if letters / len(t) < 0.75: return False
    return True
clean = sorted({l for l in labels if is_label(l)})
print("clean label-like last-segments:", len(clean))
spm_out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "spm_labels.txt")
with open(spm_out, "w", encoding="utf-8") as fh:
    fh.write("\n".join(clean))
print("wrote", spm_out)
print("clean sample (200):")
for x in clean[:200]:
    print("  ", repr(x))

# Compare against the current dictionary (plugin translations).
import json
filled = {}
for p in glob.glob(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                "..", "translations", "*_zh.json")):
    try:
        d = json.load(open(p, encoding="utf-8"))
        for k, v in d.get("translations", {}).items():
            if v.strip() and v != "_skip_":
                filled[k] = v
    except Exception:
        pass
missing = [l for l in clean if l not in filled]
print("clean labels already in dict:", len(clean) - len(missing),
      " missing:", len(missing))
print("missing sample:", missing[:120])

# True definition labels: no digits, no underscores, not asset/node names.
def is_def(l):
    if re.search(r"\d|_", l): return False
    if re.match(r"^(Ghost|Leaf|Branch|Ferns|Knots|Capsule|Cluster|Large|Details|Dirt|"
                r"Batched|Bifurcating|Cap|Danon|Hagning|Hanging|Vertex|Vine)", l):
        return False
    return is_label(l)
defs = sorted({l for l in clean if is_def(l)})
defs_missing = [l for l in defs if l not in filled]
print("\nDEFINITION labels:", len(defs), " already:", len(defs) - len(defs_missing),
      " missing:", len(defs_missing))
print("definition missing sample:", defs_missing[:150])
import collections
sl = sorted(labels)
print("sample last-segments (250):")
for x in sl[:250]:
    print("  ", repr(x))
