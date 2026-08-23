#!/usr/bin/env python3
"""Translate the node/property definition labels extracted from the .spm (gzip
XML) sample trees, merging the missing ones into a dedicated dictionary.
"""

import glob
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(HERE)
SRC_TRANS = os.path.join(PLUGIN, "translations")
NAT_TRANS = os.path.join(PLUGIN, "native", "translations")

MAP = {
    "Flexibility": "柔韧性", "General Direction": "总体方向",
    "Frond Shoots": "叶状体新梢", "Frond Stems": "叶状体茎",
    "Leeward dampening": "背风阻尼", "Lichen Base": "地衣基底",
    "Locality": "局部性", "Leaves/Fronds/Meshes": "叶片/叶状体/网格",
    "Max resolve steps": "最大解析步数", "Medium Branch": "中枝",
    "Mesh Converter": "网格转换器", "Mesh helper (end)": "网格辅助（末端）",
    "Moss Fluff": "苔藓绒毛", "MossCard": "苔藓卡片", "Noisy bark": "噪点树皮",
    "Ortho camera": "正交相机", "Projector Flowers": "投影花",
    "Projector Lichen": "投影地衣", "Projector Moss": "投影苔藓",
    "Projector Roots": "投影根", "Projector Spheres": "投影球",
    "Projector Vines": "投影藤", "Reference Flower": "参考花",
    "Reference Leaf": "参考叶", "References": "参考",
    "Restich lower LODs": "重新缝合较低 LOD", "Root Twigs": "根细枝",
    "Roots": "根", "Self-collide": "自碰撞", "Single Vine": "单藤",
    "Slide": "滑动", "Small": "小", "Small Base Twigs": "基部小细枝",
    "Small Branches": "小枝", "Small Stem": "小茎", "Small Vines": "小藤",
    "Spheres": "球体", "Spines": "棘", "Spring Stem": "弹簧茎", "Stalks": "花茎",
    "Stump": "树桩", "Top Cap": "顶部端盖", "Trunk Detail": "主干细节",
    "Trunk Width": "主干宽度", "Twig Detail": "细枝细节", "UV patch": "UV 补丁",
    "Hand Drawn Vine": "手绘藤", "Hand-drawn": "手绘", "Horizon": "地平线",
    "Initialize": "初始化", "Huge": "巨大", "Independence": "独立性",
    "Little": "小", "Little Branches": "小枝", "Long Detail High": "长细节高",
    "LongDetail Low": "长细节低", "Lump": "圆块", "Magnet": "磁体",
    "Main Branches": "主枝", "Many Vines": "多藤", "Mapping": "映射",
    "Medium Branches": "中枝", "Medium Cluster": "中簇", "Medium Stem": "中茎",
    "MediumBranches": "中枝", "Middle branch": "中枝",
    "Needle Cluster (tip)": "针叶簇（尖端）", "NoLeafBranches": "无叶枝",
    "Peduncle": "花梗", "Repulse": "排斥", "Return": "返回", "Throw": "抛掷",
    "Towards": "朝向", "Transparency": "透明度", "UV": "UV", "Undefined": "未定义",
    "Additional": "附加", "BAKE": "烘焙", "BURN": "烧灼", "Debug": "调试",
    "End Leaves": "末端叶", "FBX filename": "FBX 文件名", "FBX scale": "FBX 缩放",
    "Failures": "失败", "Fan": "扇形", "Flower Base": "花基",
    "GEOMETRY WRAP": "几何包裹", "HDRI": "HDRI", "Large": "大", "Larg Fill": "大填充",
    "Nearest whole v tile": "最近的整 V 贴片", "SEED": "种子",
    "TEXTURE BAND": "纹理带", "Seal": "密封", "Rook": "车", "NAT": "NAT",
    "SEASONS EXAMPLE LEAVES": "季节示例叶",
    "RootsExt": "根扩展", "FOV": "视场", "Tiny": "极小",
    "Tubes": "管", "RootsMain": "主根",
}


def is_def(l):
    if re.search(r"\d|_", l): return False
    if re.match(r"^(Ghost|Leaf|Branch|Ferns|Knots|Capsule|Cluster|Large|Details|Dirt|"
                r"Batched|Bifurcating|Cap|Hagning|Hanging|Vertex|Vine)", l):
        return False
    if not (2 <= len(l) <= 60): return False
    if not l[0].isalpha() or not l[0].isupper(): return False
    if re.match(r"^[-\d(]", l): return False
    if re.search(r"[{};=%\\<>|^~`#]", l): return False
    letters = sum(1 for c in l if c.isalpha() or c.isspace() or c in "(),-/'\"")
    if letters / len(l) < 0.75: return False
    return True


def main():
    lines = open(os.path.join(HERE, "scratch", "spm_labels.txt"), encoding="utf-8").read().splitlines()
    labels = {l for l in lines if is_def(l)}
    filled = {}
    for p in glob.glob(os.path.join(SRC_TRANS, "*_zh.json")):
        try:
            d = json.load(open(p, encoding="utf-8"))
            for k, v in d.get("translations", {}).items():
                if v.strip() and v != "_skip_":
                    filled[k] = v
        except Exception:
            pass
    out = {}
    # preserve any previously-written spmlabels entries
    for p in (os.path.join(SRC_TRANS, "dictionary_spmlabels_zh.json"),
              os.path.join(NAT_TRANS, "dictionary_spmlabels_zh.json")):
        if os.path.isfile(p):
            try:
                d = json.load(open(p, encoding="utf-8"))
                for k, v in d.get("translations", {}).items():
                    if v.strip() and v != "_skip_":
                        out.setdefault(k, v)
            except Exception:
                pass
    missing = []
    for l in labels:
        if l in filled: continue
        zh = MAP.get(l)
        if zh:
            out[l] = zh
        else:
            missing.append(l)
    print("definition labels:", len(labels), " missing:", len(labels) - len(filled),
          " translated:", len(out), " still-missing:", len(missing))
    print("still missing:", missing)
    payload = {
        "$schema": "sp-translation-v1", "id": "speedtree-spmlabels",
        "language": "zh-CN", "description": ".spm 解压的节点/参数定义标签（自动翻译）",
        "translations": out,
    }
    for d in (SRC_TRANS, NAT_TRANS):
        os.makedirs(d, exist_ok=True)
        p = os.path.join(d, "dictionary_spmlabels_zh.json")
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(payload, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
        print("wrote", p, len(out))


if __name__ == "__main__":
    main()
