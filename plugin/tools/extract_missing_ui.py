#!/usr/bin/env python3
"""Extract missing SpeedTree UI strings without launching SpeedTree.

The result is intentionally kept separate from the active dictionary.  This
script reads PE string literals only; it never consumes probe output, logs,
drawText captures, or any other runtime inventory.
"""

import argparse
import gzip
import html
import json
import os
import re
from datetime import datetime, timezone

from extract_complete import scan_file


HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(HERE)
DEFAULT_DICTIONARY = os.path.join(PLUGIN, "translations", "dictionary_zh.json")
DEFAULT_OUTPUT = os.path.join(HERE, "output", "missing_static_ui_zh.json")
DEFAULT_INSTALL = r"C:\Program Files\SpeedTree\SpeedTree Modeler v10.2.0\win64"

SPM_PROPERTY_NAME = re.compile(
    r"<Property\b[^>]*>\s*<Name>(.*?)</Name>", re.DOTALL | re.IGNORECASE
)
SPM_LABEL = re.compile(r"^[A-Za-z][A-Za-z0-9 ()'’,+./&-]{1,99}$")
REVIEW_UI = set("""
Export restrictions include
FCurve Matrix Converter
Generator pass too late for Projector
Ground Duration, or Material
Linear Color Interpolation
Matrix Scaling
Merge Layer and Timewarp
No anchor found
No keys
No longer supported
Noise Color Interpolation
Op Color
Op Scale
Original Scale
Original Unit Scale Factor
Sink edges until hidden, discard failures
Sink edges until hidden, keep failures
SpeedTree Pipeline SDK
Spheric XYZ
Standard atmospheres
Tools support
Twitch Freq Scale
Type Scale
U scale
Unit Scale Factor
Use Matrix From Pose
""".strip().splitlines())

# Reviewed SpeedTree-facing strings.  Keeping this explicit is deliberate:
# embedded SDKs contain thousands of plausible English phrases that cannot be
# distinguished from application UI reliably by wording alone.
CURATED_UI = set("""
Absolute From Camera
Add camera
Add new bookmark for the current directory
Add scene camera
Albedo With Generic Alpha
Alternating (distichous)
Animation data not found
Application Exit
Application is inactive
Axis system is Left Handed
Camera FBX file
Camera Elevation Angle
Camera Focal Length converted to Field Of View
Camera Index
Camera Interest
Camera Rate
Camera Switcher
Camera body
Camera facing
Cameras
Calm, occasional breezes
Calm, occasional gusts
Capture Camera
Conserve graphics card memory
Controls the projection's field of view
Convert to 'Leaf Mesh'
Convert to 'Batched Leaves'
Copy camera
Cubic meters
Cull backfaces
Current wind mode unsupported
Displacement scale
Display scaling
Display scaling preview
Do nothing (my conditions are fine)
Do nothing additional right now
Do nothing, mine are fine
Esc to exit
Exact Zero As Occluded
Exp decay
Exp growth
Export Image Sequence
Export Material
Export Summary
Export Tree Image
Export UV Areas
Export Unwrapped Mesh
Export World Building Data
Export image sequence...
Export image...
Export maps...
Export material...
Export type
Export world building data...
Exporting geometry
Exporting mesh
Euler XYZ
FBX scale
FOV
Filament Cutout
Film Height
Film Width
Feet per second squared
Focal Length
Follow us on Twitter...
Forces (temporarily disabled)
Frame Sampling Count
From Camera...
Frond size scalar
Geometry Force has no behaviors, has no effect
Gimble Killer
GlobalAxisofGravity Y
Group size scale
Grow scale
HDRI
Hide selection indicator on edit
Hierarchy not found
Hit backfaces
Icon scale
Ignore backfaces
Indicator scale
Inches per second
Inches per second squared
LOD
Layer path is empty
Lighting scalar
Licensing info...
Licensing options
Lock camera
Logging in via browser...
Loop tolerance scale
Manage scene objects (Forces, Collision Objects, Scene Cameras, and Projectors)
Master Scale
Meters per second
Meters per second squared
Miles per hour
Miles per min
Miles per sec
My Scene
New Texture Packer
No Fit
Not all channels are animated
No camera specified
No wind levels available for ribs
Noise (late)
Normals scale
Number of snap units between each grid line
Opacity (noisy)
Opacity margin
Opacity value
Opposite (decussate)
Opposite (distichous)
Override base tex coords
Open (compute disabled)...
Open Displacement
Open Image
Open Map
Open Mask
Open Mesh
Open SpeedTree Model File
Open SpeedTree Raw File
Optimize segments
Parent curve count mismatch
Panorama Adjustments
Paste camera
Peeling bark (all down)
Peeling bark (all up)
Peeling bark (up and down)
Perspective Camera
Photogrammetry Sample Mesh
Pipeline
Pivot To Null
Planar Force
Pole Vector Object
Primary Visibility
Print scene graph
Proportional steps
Purchase now...
Proportional Steps
Radial Color Interpolation
Radius scalar
Reinhard (Luminance)
Relative To Interest
Remove selected bookmark
Rename Duplicate Names
Render Photo
Render photo...
Rendering photo
Rescaling light
Roughness (1-Gloss)
Reset lightmap scalars
Restore camera
Right radius scale
Rotation scale
Save camera
Scene cameras
SEED
Sequence Camera
Set how UV coordinates are computed for texture mapping and light mapping
Slope based sink
Slot Create Geometry Force
Specular scale
Spread scalar
SpeedTree Texture Packer
Spherical Reflection Map
Split disabled (not compatible with subd models)
Streaking maps...
Subdivision
Subsurface scale
SubD branches
SubD surface (arbitrary attachments)
SubD surface (rectangular attachments)
Switch to Substances where possible
Switch to simple mesh
Texture Alpha
Texture Blend
Texture Coordinates
Texture Flip Opacity
Texture Id Count
Texture Packing Setup
Texture Rotation
Texture UV by Polygon
Texture alpha
Texture blend mode not recognized
Texture density
Texture mapping
Texture pull
Texture scale
Texture size
Time elapsed (s)
Time scale
Toggle hand drawn
Total Units Scale
Transform Scale
Transmission scalar
Tree library
Unification
Units Scale
Unwrap Scale
Up
Up Axis Used in File
Upgrade now
Use Scale Conversion
Use base tex coords
Use spherical test for leaves
User comments
V Scale
V scale
Vertical Fit
Vertical dampening
Vertical offset
View as browser
Weld Scale
Weld scalar
Width scale
Wind mode not supported
World units scale
XYZ
YouTube channel...
""".strip().splitlines())


def load_translations(path):
    with open(path, encoding="utf-8-sig") as fh:
        payload = json.load(fh)
    translations = payload.get("translations")
    if not isinstance(translations, dict):
        raise ValueError(f"invalid translation dictionary: {path}")
    return translations


def high_confidence(text):
    return text in CURATED_UI


def plausible_review(text):
    """Keep uncertain UI-like text; discard candidates with no credible UI use."""
    return text in REVIEW_UI


def read_spm(path):
    with open(path, "rb") as fh:
        raw = fh.read()
    if raw.startswith(b"\x1f\x8b"):
        raw = gzip.decompress(raw)
    return raw.decode("utf-8", "replace")


def scan_spm_files(install_root):
    """Return UI property labels stored in sample and template SPM files."""
    labels = set()
    scanned = 0
    roots = [
        os.path.join(install_root, "samples"),
        os.path.join(install_root, "tree_templates"),
    ]
    for root in roots:
        if not os.path.isdir(root):
            continue
        for current, _dirs, files in os.walk(root):
            for filename in files:
                if not filename.lower().endswith(".spm"):
                    continue
                path = os.path.join(current, filename)
                try:
                    text = read_spm(path)
                except (OSError, EOFError, gzip.BadGzipFile):
                    continue
                scanned += 1
                for full_name in SPM_PROPERTY_NAME.findall(text):
                    # Stored names are paths such as Geometry:Spine:Length. The
                    # final component is the actual label displayed by the UI.
                    label = html.unescape(full_name).split(":")[-1].strip()
                    if SPM_LABEL.fullmatch(label):
                        labels.add(label)
    return labels, scanned


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install", default=DEFAULT_INSTALL)
    parser.add_argument("--dictionary", default=DEFAULT_DICTIONARY)
    parser.add_argument("-o", "--output", default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    binaries = [
        os.path.join(args.install, "SpeedTree_Modeler.exe"),
        os.path.join(args.install, "QtAdvancedDocking.dll"),
    ]
    missing_binaries = [path for path in binaries if not os.path.isfile(path)]
    if missing_binaries:
        raise SystemExit("binary not found: " + ", ".join(missing_binaries))

    existing = load_translations(args.dictionary)
    binary_candidates = set()
    for path in binaries:
        binary_candidates.update(scan_file(path))
    install_root = os.path.dirname(args.install) if os.path.basename(args.install).lower() == "win64" else args.install
    spm_candidates, spm_files_scanned = scan_spm_files(install_root)
    candidates = binary_candidates | spm_candidates
    missing = candidates.difference(existing)

    high = sorted(text for text in missing if high_confidence(text))
    unresolved = missing.difference(high)
    review = sorted(
        text for text in unresolved
        if plausible_review(text)
    )
    discarded = unresolved.difference(review)
    result = {
        "$schema": "sp-static-missing-ui-v1",
        "description": "纯静态提取的缺失 UI 词条；不会在运行时提取",
        "generated_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "sources": [os.path.basename(path) for path in binaries] + ["samples/**/*.spm", "tree_templates/**/*.spm"],
        "dictionary": os.path.basename(args.dictionary),
        "counts": {
            "static_candidates": len(candidates),
            "binary_candidates": len(binary_candidates),
            "spm_files_scanned": spm_files_scanned,
            "spm_candidates": len(spm_candidates),
            "spm_missing": len(spm_candidates.difference(existing)),
            "already_in_dictionary": len(candidates) - len(missing),
            "translations": len(high),
            "translations_from_binary": len(set(high).intersection(binary_candidates)),
            "translations_from_spm": len(set(high).intersection(spm_candidates)),
            "review_candidates": len(review),
            "discarded_untrusted": len(discarded),
        },
        "translations": {text: "" for text in high},
        "review_candidates": review,
    }

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(result, fh, ensure_ascii=False, indent=2)
        fh.write("\n")
    print(f"wrote {args.output}")
    print(f"high confidence: {len(high)}, review: {len(review)}")


if __name__ == "__main__":
    main()
