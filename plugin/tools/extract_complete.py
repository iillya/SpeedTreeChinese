#!/usr/bin/env python3
"""Comprehensive static extraction of SpeedTree UI + node-parameter strings.

Scans the SpeedTree_Modeler.exe image end-to-end (inline literals in .text, UI
strings in .rdata/.data, both UTF-8 and UTF-16), plus QtAdvancedDocking.dll
(dock tab/menu strings), then also merges the previously captured runtime/sniff
pools so nothing seen before is lost.  Aggressive noise filtering keeps only
plausible user-facing labels and node-parameter phrases.

Outputs:
    scratch/complete_pool.txt      sorted unique candidate strings
    scratch/complete_untranslated.txt  same pool minus already-translated keys
"""

import glob
import os
import re
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
SCRATCH = os.path.join(HERE, "scratch")
INSTALL = r"C:\Program Files\SpeedTree\SpeedTree Modeler v10.2.0\win64"

CJK = re.compile(r"[\u3400-\u9FFF\uF900-\uFAFF]")
NUM = re.compile(r"^[+-]?(?:\d+(?:[.,]\d+)?|[.,]\d+)(?:[eE][+-]?\d+)?%?$")
CODE_PUNCT = re.compile(r"[#{};=%\\<>|^~`]")
PATHY = re.compile(r"[\\/]|\.[A-Za-z0-9]{1,5}\b")
URLY = re.compile(r"http|www|\.com\b|\.org\b|@|github|speedtree\.com|\.dll\b|\.exe\b|\.db\b", re.I)
CAMELL = re.compile(r"[a-z][A-Z]")
ALL_CAPS_DIGIT = re.compile(r"^[A-Z0-9_\- ]{2,}$")
ACRONYMS = {
    "LOD", "UV", "XYZ", "IK", "SDK", "FPS", "FOV", "LUT", "HDR", "TGBA",
    "TGA", "SIT", "SRT", "VSM", "AO", "GI", "FBM", "SSS", "GPG", "SLR",
    "DDS", "MIP", "GPU", "CPU", "PVS", "CRC", "PNG", "JPG", "JPG", "TIF",
    "BMP", "U", "V", "W", "X", "Y", "Z", "AI", "HD", "SD", "I/O", "ID",
}
# strings we never want (runtime logs, crypto, net, licensing, error walls)
BAD = re.compile(
    r"\b(class|const|void|enum|struct|bool|int|char|string|std|pxr|this|nullptr|"
    r"virtual|namespace|template|typedef|malloc|strcpy|acquire|release|"
    r"error|err\b|failed|fail\b|cannot|can not|unknown|internal|corrupt|assert|"
    r"invalid|not enough|missing|occured|occurred|fatal|exception|throw|catch|"
    r"service|provider|crypto|openssl|certificate|x509|intel|licens|license|"
    r"roaming|hostid|registry|win32|kernel32|advapi|netapi|unity|mac|linux|"
    r"warning|attempting|direct connection|privilege|threads|features|"
    r"variable|deny|denied|permission|access|code|version info|copyright|"
    r"filename|all rights|more information|click here|patch|tile-parts|"
    r"thousand|comma|array element|stack trace|only|safe|seememory)\b", re.I)
EXPORTER = re.compile(
    r"\b(alembic|usd|usdz|fbx|obj|exif|lumberyard|autodesk|wavefront|"
    r"universal scene|prim index|layer element|ocsp|x509|cryptograph|"
    r"certificate|staf|sic_|malloctag|rlm|debug for|no subtree|removed last dep|"
    r"dependencies|dynamic file format|macros enabled|notation|unsigned rational|"
    r"escapee|conversion exists|emptied|matching saved|straightened texture|"
    r"speedtree runtime sdk|din|digi\s+camera)\b", re.I)
SENTENCE = re.compile(
    r"\b(has been|will be|is set to|could not|can not|may not|no such|there is|"
    r"it (will|can|is)|in order to|because|given|using|when |since |"
    r"for all|and may|has occurred|one or more|a maximum)\b", re.I)
SIG_PAREN = re.compile(r"\(\s*(void|const|class|enum|int|char|bool|std|pxr|Q|float|double|unsigned|long)\b")
SINGLE_LABEL = re.compile(r"^[A-Z][a-z]{1,}$")          # Mesh, Length, Curl, Freq
DOMAIN_UI = set("""
Branch Leaf LeafCluster Frond Trunk Root Wire Generator Spine Stirp
Mesh Materials Material Texture Displacement Displace Mask MaskSet LOD
Wind Collision Curve Profile Section Vertex Normal Tangent UV Uv Map
Export Import Save Open Delete Add Remove Rename Apply Cancel Ok Confirm
Generation Geometry Lod Force Gravity Curl Freq Frequency Noise Seed
Radius Length Width Height Size Scale Rotate Rotation Twist Bend Fold
Distribution Probability Density Amount Count Level Depth Alpha Opacity
Color Colorize Ambient Diffuse Specular Bump Map Normal Bump Scale
Child Parent Children Sibling Merge Split Align Anchor Axis Global Local
Time Timeline Animation Keyframe Camera Light Render View Tools Window
File Edit Help Selection Settings Properties Output Information Advanc
Model Actions More Render Generate Add Remove Reset Show Hide Enable
Disable Use Include Exclude Overlap Offset Direction Look Align Parent
Attachment Attachment Target Connect Normal Skin Skinning Environment
Metadata Variant Layer Blendshape Skinning Weights Influence Panel
Culling Fade Near Far Distance Billboard Shadow Shader Caster Recieve
Receive Lit Uniform Instancers Randomize Variety Base Detail Blending
Overwrites Ignore Redraw Center Facing Compress Dilation Erosion Resize
Snap Grid Smooth Sharpen Strength Intensity Gain Decay Ellipsoid Cube
Sphere Cylinder Conical Homogeneous Horizontal Vertical On Off Auto
Adjust Amount Angle Value Default Increment Decrement Copy Paste Undo
Redo Selection Move Scale Rotate Translate Transform Constraint Channel
Overridden Overwrite Compose Pipeline Buffer Lod Fold Curve Segments
Stages Steps Determinism Ordering Negative Positive Multiply Divide Sum
Average Normalize Standard Units Meters Centimeters Percentage Jitter
Variation Random Phase Speed Rate Lifetime Age Growth Pseudo Force Field
Wind Speed Direction Vary Grow Strength Layers Stack Repeat Tile Skip
Inflate Puff Storm Gust Exposure Frequency Scale Map Value
Vertex Weld Flip Twist Swap Mirror Reflect Move Offset Pivot Origin
Yaw Pitch Roll Forward Up Right Down Inward Outward Sided Double Single
Absorb Bounce Friction Resistance Flow Stream Path Streak Particle
Stripe Deform Morph Blend Smooth Interpolate Lerp Curvature Arc Angle
Aspect Ratio Padding Margin Spacing Indent Gravity Friction Orientation
Sink Float Anchor Offset Gap Attach Connect Hierarchy Root Group Level
""".split())
UI_GENERIC = set("""
File Edit View Window Help Tools Selection Settings Properties Output
Information Advanced Actions More Render Model Materials Meshes Mask
Timeline Animation Scene Camera Collision Generate Generator Geometry
Dragging Rotate Scale Translate Reset Copy Paste Duplicate Delete
Clicking Wheeling Middle Double Left Right Single Add Remove Apply
Retain Option Toggle Check Enable Disable Show Hide Visible Expanded
Collapsed Expand Collapse Fold Unfold Selected Focused Preferred Default
Last First Next Previous Current Now Later Earlier Soon After Before
Between Within Without Inside Outside Up Down Over Under Above Below
Near Top Bottom Left Center Right Middle High Low Average Baseline
Individual Group Global Local Preset Custom Arrange Sort Clear
Open Save Export Import New Edit Close Build Clean Refresh Flush
Detach Pin Dock Float Tile Cascade Move Resize Maximize Minimize Restore
Horizontal Vertical Diagonal Grid Snap Axis XYZ W U V Origin Pivot
Label Tooltip Status Info Notice Warning Message Question Prompt
OK Cancel Yes No Apply Retry Ignore Abort Stop Pause Play Rewind
Undo Redo Cut Paste Copy Select All None Invert Swap Toggle Lock
Auto Manual Single Double Number Integer Float Boolean Vector Matrix
""".split())
PARAM_SUFFIX = re.compile(
    r"(Length|Radius|Seed|Count|Scale|Variance|Amount|Offset|Gravity|Force|"
    r"Color|Size|Width|Height|Angle|Weight|Density|Jitter|Rotation|Roll|Curl|"
    r"Fold|Speed|Distribution|Noise|Strength|Thickness|Curve|Profile|Bend|"
    r"Spread|Taper|Align|Pattern|Shape|Rate|Level|Depth|Factor|Amount|"
    r"Displacement|Opacity|Alpha|Ambient|Diffuse|Specular)$")


def split_camel(token: str) -> str:
    out = []
    for i, ch in enumerate(token):
        if i and ch.isupper() and token[i - 1].islower():
            out.append(" ")
        out.append(ch)
    return "".join(out).strip()


def ascii_runs(buf):
    run = bytearray()
    for b in buf:
        if 0x20 <= b <= 0x7E:
            run.append(b)
        else:
            if len(run) >= 3:
                yield bytes(run)
            run = bytearray()
    if len(run) >= 3:
        yield bytes(run)


def utf16_runs(buf):
    run = bytearray()
    i, n = 0, len(buf)
    while i + 1 < n:
        lo, hi = buf[i], buf[i + 1]
        if hi == 0 and 0x20 <= lo <= 0x7E:
            run.append(lo)
            i += 2
        else:
            if len(run) >= 4:
                yield bytes(run)
            run = bytearray()
            i += 1
    if len(run) >= 4:
        yield bytes(run)


def camel_runs(buf, min_len=6):
    run = bytearray()
    for b in buf:
        if 0x41 <= b <= 0x5A or 0x61 <= b <= 0x7A:
            run.append(b)
        else:
            if len(run) >= min_len:
                yield bytes(run)
            run = bytearray()
    if len(run) >= min_len:
        yield bytes(run)


def is_label(s) -> bool:
    t = s.strip()
    if t != s:
        return False
    if not (2 <= len(t) <= 100):
        return False
    if CJK.search(t):
        return False
    if NUM.match(t):
        return False
    if not re.search(r"[A-Za-z]", t):
        return False
    if not t[0].isalpha() or not t[0].isupper():
        return False
    if CODE_PUNCT.search(t):
        return False
    # sentence/symbol chars (permit only a trailing "..." ellipsis)
    body = t
    if body.endswith("..."):
        body = body[:-3]
    if re.search(r"[.!?+=*$&\[\]@]", body):
        return False
    if re.search(r"[!]|\.\.\.\.", t):
        return False
    if PATHY.search(t):
        return False
    if URLY.search(t):
        return False
    if ALL_CAPS_DIGIT.match(t) and not re.search(r"(?<![A-Z])[A-Z][a-z]", t):
        # pure uppercase/digit token (icons, ABI names, hex-ish) — drop
        if re.search(r"\d", t):
            return False
    if BAD.search(t):
        return False
    if EXPORTER.search(t):
        return False
    if SENTENCE.search(t):
        return False
    if SIG_PAREN.search(t):
        return False
    # identifiers without spaces, e.g. BranchLength / AFAreaHeight -> drop
    if " " not in t and CAMELL.search(t):
        return False
    # single word: keep real labels / acronyms, drop bare letters (A, B)
    if " " not in t:
        if not re.match(r"^[A-Z][A-Za-z]{2,}$", t):
            return False
    # a word mixing letter+digit in a junk way (A9A, AFi5) -> reject
    for w in t.split():
        if re.search(r"[A-Za-z]\d[A-Za-z]", w):
            return False
        if re.search(r"^\d+[A-Za-z]+$", w):
            return False
    # single-letter tokens that are not coordinate axes -> drop (U A, V B)
    for w in t.split():
        if len(w) == 1 and w not in ("U", "V", "W", "X", "Y", "Z"):
            return False
    # All-CAPS words (ACTOR, ADDR, ABCDEFG...) are binary/code tokens, not UI.
    # Title-case words (Actor, Address, Alpha) are the real labels.
    for w in t.split():
        if len(w) >= 2 and w.isupper() and w not in ACRONYMS:
            return False
    letters = sum(1 for c in t if c.isalpha() or c.isspace() or c in "(),-\u2019'")
    if letters / len(t) < 0.78:
        return False
    return True


def is_domain_relevant(s) -> bool:
    """Keep only strings that mention a SpeedTree or generic-UI concept, so the
    embedded third-party exporter/.NET/licensing labels are pruned away."""
    words = re.split(r"[^A-Za-z]+", s)
    for w in words:
        if w in DOMAIN_UI or w in UI_GENERIC:
            return True
    return False


def sections(data):
    if len(data) < 0x40:
        return [data]
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        return [data]
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
            out.append(data[raw_ptr:raw_ptr + min(raw_size, vsize or raw_size)])
    return out or [data]


def scan_file(path):
    try:
        with open(path, "rb") as fh:
            data = fh.read()
    except Exception:
        return set()
    cand = set()
    for buf in sections(data):
        for run in ascii_runs(buf):
            s = run.decode("ascii", "replace")
            if ":" in s and s.count(":") >= 1:
                for seg in s.split(":"):
                    seg = seg.strip()
                    if seg and is_label(seg):
                        cand.add(seg)
            elif is_label(s):
                cand.add(s)
        for run in utf16_runs(buf):
            s = run.decode("ascii", "replace").strip()
            if s and is_label(s):
                cand.add(s)
        for tok in camel_runs(buf):
            ts = tok.decode("ascii", "replace")
            if PARAM_SUFFIX.search(ts):
                derived = split_camel(ts)
                if is_label(derived):
                    cand.add(derived)
    return cand


def merge_scratch_pools():
    cand = set()
    for name in ("static_ui_candidates.txt", "node_params_human.txt",
                 "node_params_new.txt", "domain_remaining.txt",
                 "remaining_to_translate.txt", "node_params_remaining.txt",
                 "clean_full.txt"):
        p = os.path.join(SCRATCH, name)
        if os.path.isfile(p):
            for x in open(p, encoding="utf-8", errors="ignore"):
                s = x.strip()
                if s and is_label(s):
                    cand.add(s)
    return cand


def main():
    os.makedirs(SCRATCH, exist_ok=True)
    pool = set()
    for f in (os.path.join(INSTALL, "SpeedTree_Modeler.exe"),
              os.path.join(INSTALL, "QtAdvancedDocking.dll")):
        pool |= scan_file(f)
        print("scanned", os.path.basename(f), "-> candidates", len(pool))
    pool |= merge_scratch_pools()
    pool = {x for x in pool if is_label(x)}
    ordered = sorted(pool)
    with open(os.path.join(SCRATCH, "complete_pool.txt"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(ordered))
    print("TOTAL clean pool:", len(ordered))

    # existing translations
    filled = {}
    for p in glob.glob(os.path.join(HERE, "..", "translations", "*_zh.json")):
        try:
            d = json_load(p)
            for k, v in d.get("translations", {}).items():
                if v.strip() and v != "_skip_":
                    filled[k] = v
        except Exception:
            pass
    untr = [x for x in ordered if x not in filled]
    with open(os.path.join(SCRATCH, "complete_untranslated.txt"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(untr))
    print("already translated:", len(ordered) - len(untr), " remaining:", len(untr))


def json_load(p):
    import json
    with open(p, encoding="utf-8-sig") as fh:
        return json.load(fh)


if __name__ == "__main__":
    main()
