#!/usr/bin/env python3
"""Translate the reviewed offline UI inventory and produce a merge batch."""

import html
import json
import os
import time
import urllib.parse
import urllib.request


HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, "output", "missing_static_ui_zh.json")
CACHE = os.path.join(HERE, "output", "missing_static_ui_translation_cache.json")
OUTPUT = os.path.join(HERE, "manual", "manual_batch_missing_static.json")

OVERRIDES = {
    "Absolute From Camera": "相对于相机的绝对值",
    "Alternating (distichous)": "互生（二列式）",
    "Axis system is Left Handed": "坐标系为左手系",
    "Calm, occasional breezes": "平静，偶有微风",
    "Calm, occasional gusts": "平静，偶有阵风",
    "Camera Interest": "相机注视目标",
    "Camera Rate": "相机帧率",
    "Camera Switcher": "相机切换器",
    "Camera body": "相机主体",
    "Camera facing": "相机朝向",
    "Cameras": "相机",
    "Capture Camera": "捕获相机",
    "Convert to 'Batched Leaves'": "转换为“批处理叶片”",
    "Convert to 'Leaf Mesh'": "转换为“叶片网格”",
    "Copy camera": "复制相机",
    "Cubic meters": "立方米",
    "Current wind mode unsupported": "不支持当前风力模式",
    "Displacement scale": "位移缩放",
    "Do nothing (my conditions are fine)": "不执行操作（当前条件正常）",
    "Do nothing additional right now": "暂不执行其他操作",
    "Do nothing, mine are fine": "不执行操作，我的设置正常",
    "Esc to exit": "按 Esc 退出",
    "Exact Zero As Occluded": "将精确零值视为遮挡",
    "Exp decay": "指数衰减",
    "Exp growth": "指数增长",
    "Export Material": "导出材质",
    "Export Unwrapped Mesh": "导出展开后的网格",
    "Export World Building Data": "导出世界构建数据",
    "Export maps...": "导出贴图…",
    "Export material...": "导出材质…",
    "Export restrictions include": "导出限制包括",
    "Export world building data...": "导出世界构建数据…",
    "FBX scale": "FBX 缩放",
    "Filament Cutout": "丝状镂空",
    "Follow us on Twitter...": "在 Twitter 上关注我们…",
    "Forces (temporarily disabled)": "力（暂时禁用）",
    "Frond size scalar": "叶状枝尺寸系数",
    "Generator pass too late for Projector": "生成器阶段晚于投影器所需阶段",
    "Gimble Killer": "万向节锁定消除",
    "GlobalAxisofGravity Y": "全局重力轴 Y",
    "Ground Duration, or Material": "地面、持续时间或材质",
    "Group size scale": "组尺寸缩放",
    "Grow scale": "生长缩放",
    "Hit backfaces": "命中背面",
    "Icon scale": "图标缩放",
    "Inches per second": "英寸/秒",
    "Inches per second squared": "英寸/秒²",
    "Indicator scale": "指示器缩放",
    "LOD": "LOD",
    "Lighting scalar": "照明系数",
    "Loop tolerance scale": "循环容差缩放",
    "Master Scale": "整体缩放",
    "Miles per min": "英里/分钟",
    "Miles per sec": "英里/秒",
    "No Fit": "不适配",
    "No anchor found": "未找到锚点",
    "No keys": "无关键帧",
    "No wind levels available for ribs": "肋条没有可用的风力层级",
    "Noise (late)": "噪声（后期）",
    "Normals scale": "法线缩放",
    "Not all channels are animated": "并非所有通道都有动画",
    "Op Color": "操作颜色",
    "Op Scale": "操作缩放",
    "Opacity (noisy)": "不透明度（含噪声）",
    "Open Displacement": "打开位移",
    "Open Map": "打开贴图",
    "Opposite (decussate)": "对生（十字式）",
    "Opposite (distichous)": "对生（二列式）",
    "Optimize segments": "优化分段",
    "Original Scale": "原始缩放",
    "Override base tex coords": "覆盖基础纹理坐标",
    "Parent curve count mismatch": "父曲线数量不匹配",
    "Peeling bark (all down)": "树皮剥落（全部向下）",
    "Peeling bark (all up)": "树皮剥落（全部向上）",
    "Peeling bark (up and down)": "树皮剥落（上下双向）",
    "Perspective Camera": "透视相机",
    "Pipeline": "管线",
    "Pivot To Null": "枢轴归零",
    "Pole Vector Object": "极向量对象",
    "Primary Visibility": "主可见性",
    "Relative To Interest": "相对于注视目标",
    "Remove selected bookmark": "移除所选书签",
    "Rescaling light": "正在缩放灯光",
    "Right radius scale": "右侧半径缩放",
    "Rotation scale": "旋转缩放",
    "SEED": "随机种子",
    "Sink edges until hidden, discard failures": "下沉边缘直至隐藏，丢弃失败项",
    "Sink edges until hidden, keep failures": "下沉边缘直至隐藏，保留失败项",
    "Slope based sink": "基于坡度下沉",
    "Specular scale": "镜面反射缩放",
    "SpeedTree Pipeline SDK": "SpeedTree 管线 SDK",
    "Spheric XYZ": "球面 XYZ",
    "Split disabled (not compatible with subd models)": "已禁用拆分（与 SubD 模型不兼容）",
    "Spread scalar": "扩散系数",
    "Standard atmospheres": "标准大气压",
    "Streaking maps...": "条纹贴图…",
    "SubD branches": "SubD 分支",
    "SubD surface (arbitrary attachments)": "SubD 表面（任意附着）",
    "SubD surface (rectangular attachments)": "SubD 表面（矩形附着）",
    "Subdivision": "细分",
    "Switch to Substances where possible": "尽可能切换为 Substance 材质",
    "Texture Alpha": "纹理 Alpha",
    "Texture Coordinates": "纹理坐标",
    "Texture Id Count": "纹理 ID 数量",
    "Texture UV by Polygon": "按多边形计算纹理 UV",
    "Texture alpha": "纹理 Alpha",
    "Texture mapping": "纹理映射",
    "Texture pull": "纹理拉伸",
    "Texture scale": "纹理缩放",
    "Time elapsed (s)": "已用时间（秒）",
    "Total Units Scale": "总单位缩放",
    "Transmission scalar": "透射系数",
    "Twitch Freq Scale": "摇曳频率缩放",
    "U scale": "U 缩放",
    "Unification": "统一",
    "Unit Scale Factor": "单位缩放系数",
    "Units Scale": "单位缩放",
    "Use base tex coords": "使用基础纹理坐标",
    "V Scale": "V 缩放",
    "V scale": "V 缩放",
    "View as browser": "以浏览器方式查看",
    "Weld Scale": "焊接缩放",
    "Weld scalar": "焊接系数",
    "Width scale": "宽度缩放",
    "World units scale": "世界单位缩放",
    "YouTube channel...": "YouTube 频道…",
}


def load(path, default):
    try:
        with open(path, encoding="utf-8-sig") as fh:
            return json.load(fh)
    except (OSError, json.JSONDecodeError):
        return default


def save(path, value):
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(value, fh, ensure_ascii=False, indent=2)
        fh.write("\n")


def translate(text):
    query = urllib.parse.urlencode({"q": text, "langpair": "en|zh-CN"})
    request = urllib.request.Request(
        "https://api.mymemory.translated.net/get?" + query,
        headers={"User-Agent": "SpeedTreeChinese-offline-dictionary-builder/1.0"},
    )
    with urllib.request.urlopen(request, timeout=20) as response:
        payload = json.load(response)
    translated = html.unescape(payload["responseData"]["translatedText"]).strip()
    if not translated:
        raise RuntimeError("translation service returned an empty value")
    return translated


def main():
    source = load(SOURCE, {})
    keys = sorted(set(source.get("translations", {})) | set(source.get("review_candidates", [])))
    cache = load(CACHE, {})
    for index, key in enumerate(keys, 1):
        if key in cache and cache[key].strip():
            continue
        try:
            cache[key] = translate(key)
            save(CACHE, cache)
            print(f"[{index}/{len(keys)}] {key}")
            time.sleep(0.15)
        except Exception as exc:
            print(f"stopped at {key}: {exc}")
            break
    completed = {key: OVERRIDES.get(key, cache[key]) for key in keys if cache.get(key, "").strip()}
    # apply_manual.py consumes manual_batch_*.json as a direct key/value map.
    save(OUTPUT, completed)
    print(f"completed {len(completed)}/{len(keys)} -> {OUTPUT}")


if __name__ == "__main__":
    main()
