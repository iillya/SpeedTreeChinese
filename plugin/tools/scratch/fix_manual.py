#!/usr/bin/env python3
import re

p = "plugin/tools/manual_zh.py"
txt = open(p, encoding="utf-8").read()
pairs = re.findall(r'^\s*"([^"]+)":\s*"([^"]*)",?\s*$', txt, re.M)
d = {}
for k, v in pairs:
    if k not in d:
        d[k] = v
print("recovered pairs:", len(d))
header = (
    '#!/usr/bin/env python3\n'
    '"""Hand-authored (人工精翻) translations, accumulated batch by batch.\n'
    'Each entry is written by a human-quality pass for the SpeedTree context.\n'
    'apply_manual.py merges these into dictionary_zh.json, replacing any\n'
    'machine-composed value for these keys.\n'
    '"""\n\n'
    "MANUAL = {\n"
)
body = "".join("    %r: %r,\n" % (k, v) for k, v in d.items())
out = header + body + '}\n\nif __name__ == "__main__":\n    print("MANUAL entries:", len(MANUAL))\n'
open(p, "w", encoding="utf-8").write(out)
print("rewrote manual_zh.py")
