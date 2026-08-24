#!/usr/bin/env python3
"""Translate the .spm node/generator display names (curated technical term map)
and merge them into the official dictionary.  Reads build/_spmnames.txt."""

import json
import os
import re
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = os.path.join(ROOT, "build", "translations", "_spm_remaining.txt")
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
    # second expansion (words seen in the remaining .spm display names)
    "Cutout": "镂空", "Cutouts": "镂空", "Blade": "叶片", "Blades": "叶片",
    "Bud": "芽", "Chunk": "块", "Stem": "茎", "Petals": "花瓣", "Petal": "花瓣",
    "Cluster": "簇", "Needle": "针叶", "Needles": "针叶", "Moss": "苔藓", "Moss_Atlas": "苔藓图集",
    "Flower": "花", "Flower_Atlas": "花图集", "Oak": "橡树", "Oak_Bud": "橡树芽",
    "White_Oak": "白橡", "Redbud": "紫荆", "English": "英国", "Eastern": "东部",
    "Front": "正面", "Front_Season": "正季节", "Ferns": "蕨", "Fern": "蕨",
    "Ghost": "幽灵", "Ghosts": "幽灵", "KindaOld": "旧一些", "Younger": "较嫩", "Youngish": "稍嫩",
    "Temp": "临时", "Field": "田野", "Forest": "森林", "BigBranches": "大枝", "SWITCH": "分支状态",
    "Vine": "藤", "Vines": "藤", "Hanging": "悬挂", "Hagning": "悬挂", "Tendrils": "卷须",
    "Twig": "细枝", "Knot": "节疤", "Knots": "节疤", "Mature": "成熟", "Sapling": "幼树",
    "Biome": "生态区", "Fire": "火", "Flooring": "地板", "Bounce": "反弹", "Both": "两者",
    "Spread": "扩展", "Target": "目标", "Scars": "伤疤", "bumps": "凸起", "Seeds": "种子",
    "radial": "径向", "Relax": "松弛", "Ripple": "波纹", "Iteration": "迭代", "Inherited": "继承",
    "weight": "权重", "Upper": "上方", "Lower": "下方", "Main": "主", "middle": "中间",
    "Ortho": "正交", "Orthographic": "正交", "boxes": "立方体", "sphere": "球体",
    "attract": "吸引", "first": "先", "second": "再", "grow": "生长", "fill": "填充",
    "Fill": "填充", "Conversion-Tools": "转换工具", "Decidous": "落叶", "Dirt": "泥土",
    "Summer": "夏", "Spring": "春", "Fall": "秋", "Winter": "冬", "Season": "季节",
    "Decorative": "装饰", "Mesh": "网格", "Weed": "杂草", "Weeds": "杂草", "umbrella": "伞",
    "Weathered": "风化", "base": "基础", "branch": "枝", "branches": "枝", "roots": "根",
    "Scars_and": "伤疤与", "parent curve": "父曲线", "variance": "方差", "uniform": "统一",
    # third expansion (words seen as leftover in the audited values)
    "Big": "大", "Little": "小", "tree": "树", "detail": "细节", "Reference": "参考",
    "scale": "缩放", "BatchedLeaf": "批叶", "Dead": "枯", "Shoots": "芽", "Peduncle": "花梗",
    "Root": "根", "BigRoots": "大根", "Middle": "中", "Single": "单", "VineBase": "藤基",
    "offset": "偏移", "tips": "端部", "small": "小", "Drooping": "下垂", "BrokenBark": "破碎树皮",
    "Larg": "大", "LeafBranch": "叶枝", "LeafTwigs": "叶梢", "SpanishMoss": "西班牙苔藓",
    "End": "末端", "GEOMETRY": "几何", "TEXTURE": "纹理", "SEASONS": "季节", "EXAMPLE": "示例",
    "LEAVES": "叶片", "NAT": "自然", "LOW": "低", "BURN": "烧灼", "Switch": "分支状态",
    "Poly": "多边形", "mesh": "网格", "tip": "尖端", "camera": "相机", "style": "样式",
    "font": "字体", "weight": "权重", "Converter": "转换器", "WRAP": "包裹", "BAND": "条带",
    "Dead tree": "枯树", "Mature tree": "成熟树", "Mesh detail": "网格细节",
    "GEOMETRY WRAP": "几何包裹", "TEXTURE BAND": "纹理条带",
    "SEASONS EXAMPLE LEAVES": "季节示例叶片", "Spark": "火花", "Web": "网",
}


def _skip(s):
    """Strings we should never translate (files, dates, versions, internal codes)."""
    if re.search(r"\.(png|jpg|jpeg|tga|obj|fbx|hdr|tex|dds)$", s, re.I): return True
    if re.match(r"(Mon|Tue|Wed|Thu|Fri|Sat|Sun) \w+ \d+ \d{4}", s): return True
    if re.search(r"\bModeler\b|\bVersion\b|\bv\d+\.\d+", s): return True
    if re.fullmatch(r"[A-Z][A-Z_A-Z0-9]{3,}", s) and "_" in s: return True   # MATURE_LOW, STUMP_BURN
    if s.upper() == s and len(s) >= 3 and not s.isdigit(): return True      # NAT, SAPLING_VFX
    if re.fullmatch(r"^[a-z]{2,6}$", s): return True   # st9, box, rook (lowercase code-ish)
    if s in ("Quercus robur",): return True            # Latin species name
    if re.search(r"[()]", s): return True              # "Modeler v9.0.0 RC 3 (Cinema Edition)"
    if "_" in s and " " not in s: return True          # concatenated internal IDs (GhostOld_Field_Trunk)
    return False


def _root(w):
    w = w.strip()
    # exact match, or a plural "Name" -> "Names"
    if w in WORD:
        return w
    if w.rstrip("s") in WORD:
        return w.rstrip("s")
    return w


def translate_name(s):
    # e.g. "Bifurcating 2->Leaves 2" / "Base-&gt;Decal 4"
    s = s.replace("-&gt;", "->")
    parts = re.split(r"([^A-Za-z]+)", s)
    out = []
    for p in parts:
        if not re.search(r"[A-Za-z]", p):
            out.append("→" if p == "->" else p)
            continue
        # translate the alphabetic root, keep trailing digits
        m = re.match(r"([A-Za-z]+)([A-Za-z0-9._-]*)", p)
        word = m.group(1); tail = m.group(2) or ""
        root = _root(word)
        tr = WORD.get(root) or WORD.get(word)
        out.append((tr if tr else word) + tail)
    return "".join(out)


def main():
    maind = json.load(open(MAIN, encoding="utf-8"))
    trans = maind.setdefault("translations", {})
    added = 0
    with open(NAMES, encoding="utf-8") as fh:
        for line in fh:
            s = line.strip()
            if not s or s in trans or _skip(s):
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
