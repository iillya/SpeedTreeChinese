#!/usr/bin/env python3
"""Merge the ST_TRPROBE-captured untranslated entries into the official dictionary.

Reads translations/untranslated_captured_zh.json (ordered candidates), pairs each
source with an index-aligned Chinese translation below, and adds non-empty ones
into translations/dictionary_zh.json (then copies to build/translations).

Run:  python scripts/merge_captured.py
"""

import json
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "translations", "untranslated_captured_zh.json")
MAIN = os.path.join(ROOT, "translations", "dictionary_zh.json")
BUILD = os.path.join(ROOT, "build", "translations", "dictionary_zh.json")

# Index-aligned Chinese translations for the 134 captured sources ('' => skip).
# Short labels / options / help text translated; axis labels, shortcuts, arrows,
# bare "..." and an empty HTML blob left as-is (skipped).
TR = [
    "1 - 适用于新建的窗口。",
    "2 - 必须启用才能让 alpha-to-coverage（“最佳可用”透明度设置）生效。仅当显卡驱动允许应用控制其抗锯齿设置时有效。",
    "<html><head/><body><p>启动</p><p>系统信息（操作系统等）</p><p>产品信息</p></body></html>",
    "<html><head/><body><p>地区 / 国家</p><p>GPU 信息</p><p>导出统计</p></body></html>",
    "<html><head/><body><p>SpeedTree Modeler 可报告其使用情况数据以帮助我们改进。所报告的所有信息均为完全匿名，不会传输给第三方。所报告的信息包括：</p></body></html>",
    "允许匿名上报使用数据",
    "始终使用系统原生文件对话框",
    "询问",
    "Autodesk 3ds Max",
    "Autodesk Maya",
    "自动保存",
    "厘米",
    "英寸",
    "千米",
    "米",
    "英里",
    "多重采样",
    "导航",
    "纹素密度目标（每单位纹素数）",
    "当风开启时",
    "码",
    "您的隐私对我们很重要。如果您不希望发送使用数据，可用下面的选项关闭此功能。",
    "",
    "复制/粘贴菜单。",
    "选择要编辑的位移贴图。\n位移贴图可应用于树枝几何体。",
    "根据相对高度限制生长（0.0 = 底部，1.0 = 顶部）。",
    "选择要编辑的遮罩。\n遮罩应用于“生成”分组中，以控制子对象可以生长的位置。",
    "纹理坐标",
    "镂空/网格",
    "导出…",
    "导入…",
    "选择要编辑的材质。\n材质通过拖放或从材质属性中选择来应用到几何体。",
    "变体",
    "成员",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "根据 UV 面积计算缩放",
    "复制/粘贴菜单…",
    "编辑网格…",
    "确保每个面的绕向与表面法线匹配（确保实时渲染中正确的面剔除）。",
    "导出此网格…",
    "围绕指定轴翻转模型。常用于转换坐标系并对齐网格，以便用作叶片或树叶。",
    "为获得最佳效果，请使用“方向”和“翻转”选项来按图中所示用途调整网格的方向。",
    "最低 LOD 网格文件。",
    "中间 LOD 网格文件。",
    "如有必要，重新调整网格方向。常用于转换坐标系并对齐网格，以便用作叶片或树叶。",
    "报告网格中的顶点数量。",
    "选择要编辑的网格。\n网格可用作叶片、树叶和地形几何体，也可与力网格一起塑形树木几何体或成为模型的一部分。",
    "指定网格文件及其应应用的处理方式。",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "错误和警告",
    "仅错误",
    "错误、警告和提示",
    "消息",
    "调整 (+/-)",
    "",
    "<html><head/><body><p>不要选择法线背向相机的顶点</p></body></html>",
    "<html><head/><body><p>按住空格进行笔刷选择：<br/>左键 = 设置选择<br/>Ctrl+左键 或 Shift+左键 = 添加到选择<br/>Alt+左键 或 右键 = 移出选择<br/><br/></p></body></html>",
    "<html><head/><body><p>仅选择靠近上一次选择的顶点（下次滑动时重置）</p></body></html>",
    "清除所选及其后代",
    "转换",
    "模式提示",
    "第 2 步：按住空格，然后点击并拖动末端的菱形以描绘枝干形状。",
    "游戏",
    "学习",
    "启动时显示此对话框",
    "SpeedTree",
    "VFX",
    "在此输入许可证",
    "错误详情",
    "强制重新登录",
    "许可证类型",
    "登录信息",
    "使用 Unity ID 登录",
    "登录…",
    "申请许可证…",
    "SpeedTree 许可",
    "使用许可证",
    "v10",
    "分辨率 / LOD",
    "",
    "",
    "编辑…",
    "&层叠树窗口",
    "&检查更新…",
    "&控制台",
    "&导出网格…",
    "&生成编辑器",
    "&新建…",
    "&首选项…",
    "&购买许可证…",
    "&平铺树窗口",
    "&树木信息…",
    "",
    "调试 XML 文件…",
    "导&出所选为网格…",
    "退出 SpeedTree Modeler 应用程序",
    "从 FBX 导入相机…",
    "从 FBX 导入多个相机",
    "打开新的 SpeedTree 文件",
    "另&存为…",
    "UE4 集成问题支持…",
    "停靠栏",
    "<html><head/><body><p>用此值全局控制树的生长速度。每个生成器/节点都有一个属性可在模型内局部缩放此值。</p></body></html>",
    "<html><head/><body><p>当前帧</p></body></html>",
    "<html><head/><body><p>播放</p></body></html>",
    "<html><head/><body><p>在生长动画期间，某些建模功能可能无法工作或行为异常。使用此功能可查看这些值并将其自动设置为适合生长的值。</p><p><br/></p><p><span style=\" font-weight:600;\">注意：</span>当你知道需要导出生长序列时，最好在启用生长的情况下编辑模型。</p></body></html>",
    "<html><head/><body><p>停止</p></body></html>",
    "<html><head/><body><p>动画的最后一帧</p></body></html>",
    "<html><head/><body><p>时间线</p></body></html>",
    "<html><head/><body><p>使用此曲线来控制生长动画的整体时间</p></body></html>",
    "帧率",
    "播放",
    "所有帧",
]


def main():
    cand = json.load(open(SRC, encoding="utf-8"))["translations"]
    keys = list(cand.keys())
    if len(keys) != len(TR):
        sys.exit(f"candidate count {len(keys)} != translation count {len(TR)}")

    main_dict = json.load(open(MAIN, encoding="utf-8"))
    trans = main_dict.setdefault("translations", {})
    added = 0
    missing = [i for i in range(len(keys))]
    for i, k in enumerate(keys):
        v = TR[i]
        if not v or v in (k,):
            continue
        if k in trans:
            continue
        trans[k] = v
        added += 1
        missing.remove(i)

    with open(MAIN, "w", encoding="utf-8") as fh:
        json.dump(main_dict, fh, ensure_ascii=False, indent=2)
    os.makedirs(os.path.join(ROOT, "build", "translations"), exist_ok=True)
    shutil.copyfile(MAIN, BUILD)

    print(f"added {added} entries to {MAIN}")
    print(f"skipped/technical: {len(missing)}")
    skipped = [i for i in missing]
    if skipped:
        print("skipped indices:", skipped)


if __name__ == "__main__":
    import sys
    main()
