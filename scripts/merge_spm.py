#!/usr/bin/env python3
"""Translate the .spm node/generator display names (curated technical term map)
and merge them into the official dictionary.  Reads build/_spmnames.txt."""

import json
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = os.path.join(ROOT, "build", "translations", "_spmnames.txt")
MAIN = os.path.join(ROOT, "translations", "dictionary_zh.json")

WORD = {
    "Bifurcating": "分叉", "Batched": "批处理", "Batched Leaf": "批叶", "Leaf": "叶",
    "Leaves": "叶片", "Frond": "阔叶", "Fronds": "阔叶", "Mesh": "网格", "Meshes": "网格",
    "Lump": "块", "Shell": "外壳", "Cap": "端帽", "Base": "基础", "Roots": "根",
    "RootsMain": "主根", "Tubes": "管", "Tube": "管", "Stalk": "茎", "Berry": "浆果",
    "twigs": "细枝", "Twigs": "细枝", "Branch": "枝", "Branches": "枝", "Trunk": "树干",
    "Spine": "脊柱", "Spines": "脊柱", "Caps": "端帽", "Leaf Mesh": "叶网格",
    "Big Branches": "大树枝", "Little Branches": "小树枝", "Large": "大", "Small": "小",
    "Medium": "中", "MediumBranches": "中枝", "Details": "细节", "Decal": "贴花",
    "Dirt": "泥土", "Lichen": "地衣", "Weeds": "杂草", "Grass": "草", "Bush": "灌木",
    "Tree": "树", "Vine": "藤", "Stump": "树桩", "Cluster": "簇", "Sapling": "幼树",
    "LeafCluster": "叶簇", "Palm": "棕榈", "Pine": "松", "Conifer": "针叶", "Seasons": "季节",
    "Season": "季节", "Growth": "生长", "Wind": "风", "WindWeights": "风权重",
    "Lightmap": "光照贴图", "Bake": "烘焙", "Baked": "已烘焙", "Baking": "烘焙",
    "Mesh Detail": "网格细节", "Anchor": "锚点", "Anchor2": "锚点2", "Stitch": "缝合",
    "Splice": "拼接", "Split": "拆分", "Compose": "合成", "Convert": "转换",
    "ConvertTo": "转换为", "Variance": "方差", "Behavior": "行为", "Behavior 1": "行为 1",
    "Bump": "凹凸", "Burn": "烧灼", "Normal": "法线", "Height": "高度", "Alpha": "Alpha",
    "Subd": "细分", "Subdivision": "细分", "Surface": "表面", "Photogrammetry": "摄影测量",
    "Displacement": "置换", "DecalAndMesh": "贴花与网格", "MeshDetail": "网格细节",
    "ConvertInPlace": "原位转换", "Detach": "分离", "Ground": "地面",
}


def _root(w):
    w = w.strip()
    # strip trailing number/suffix like "2", " 1"
    m = None
    for key in sorted(WORD, key=len, reverse=True):
        if w == key or w.startswith(key) or w == key + "s":
            return key
    return w


def translate_name(s):
    # e.g. "Bifurcating 2->Leaves 2" / "Base-&gt;Decal 4"
    s = s.replace("-&gt;", "->")
    parts = s.split("->")
    out = []
    for p in parts:
        p = p.strip()
        # split "Name Number" -> translate Name, keep number
        sp = p.rsplit(" ", 1)
        word, num = (sp[0], sp[1]) if len(sp) == 2 and sp[1].isdigit() else (p, "")
        root = _root(word)
        tr = WORD.get(root) or WORD.get(word)
        seg = (tr if tr else word) + ((" " + num) if num else "")
        out.append(seg)
    return "->".join(out)


def main():
    maind = json.load(open(MAIN, encoding="utf-8"))
    trans = maind.setdefault("translations", {})
    added = 0
    with open(NAMES, encoding="utf-8") as fh:
        for line in fh:
            s = line.strip()
            if not s or s in trans:
                continue
            zh = translate_name(s)
            if zh and zh != s:
                trans[s] = zh
                added += 1
    json.dump(maind, open(MAIN, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    shutil.copyfile(MAIN, os.path.join(ROOT, "build", "translations", "dictionary_zh.json"))
    print(f"added {added} .spm names; dictionary now {len(trans)} entries")


if __name__ == "__main__":
    main()
