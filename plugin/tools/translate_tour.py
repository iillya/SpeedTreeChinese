#!/usr/bin/env python3
"""Translate the runtime UI-tour capture (tour_untranslated_zh.json) and merge it
into a dedicated dictionary.  Keys are matched by collapsed-whitespace text so the
exact original key (with newlines/tabs) is preserved for lookup.
"""

import json
import os
import re
import glob

HERE = os.path.dirname(os.path.abspath(__file__))
PLUGIN = os.path.dirname(HERE)
SRC_TRANS = os.path.join(PLUGIN, "translations")
NAT_TRANS = os.path.join(PLUGIN, "native", "translations")
MAIN = os.path.join(SRC_TRANS, "dictionary_zh.json")

# collapsed English -> Chinese
MAP = {
    "As:": "作为：",
    "Draft": "草稿",
    "Handling": "处理",
    "MDI": "MDI",
    "Post": "后处理",
    "Orient": "定向",
    "Redistribute": "重新分配",
    "Two-sided": "双面",
    "all frames": "所有帧",
    "to": "到",
    "- None": "- 无",
    "None": "无",
    "Copy/Paste menu.": "复制/粘贴菜单。",
    "Copy/Paste menu...": "复制/粘贴菜单…",
    "Edit name...": "编辑名称…",
    "Export this mesh...": "导出此网格…",
    "Facebook...": "Facebook…",
    "SpeedTree.com...": "SpeedTree.com…",
    "Video tutorials…": "视频教程…",
    "Purchase now…": "立即购买…",
    "Wizard...": "向导…",
    "Reload this file.": "重新载入此文件。",
    "Reload this mesh.": "重新载入此网格。",
    "Undo child material change": "撤销子材质更改",
    "Support for UE4 integration issues...": "UE4 集成问题支持…",
    "Branches\t3": "枝条\t3",
    "Fronds\t2": "叶\t2",
    "Leaves\t1": "叶片\t1",
    "Proxies\t9": "代理\t9",
    "Targets\t5": "目标\t5",
    "Zones / Meshes (no wind)\t4": "区域 / 网格（无风）\t4",
    "V:16  F:24  A:8": "顶点:16  面:24  面积:8",
    "Access collision object functions. Collision objects are simple geometric primitives that can be accessed via the SDK to assist with collision at run-time. Hot key: 7 (toggles collision objects in the viewport)":
        "访问碰撞对象功能。碰撞对象是简单的几何图元，可通过 SDK 访问以在运行时辅助碰撞。快捷键：7（在视口中切换碰撞对象）",
    "Access force functions. Forces are modeling tools that manipulate growth in world-space. Examples of their use include gravity (downward direction forces) and growing geometry around 3d geometry (geometry forces) Hot key: 6 (toggles forces in the viewport)":
        "访问力功能。力是塑造世界空间中生长的建模工具，例如重力（向下方向的力）以及围绕三维几何生长几何体（几何力）。快捷键：6（在视口中切换力）",
    "Access high level wind functions. Wind is controlled by a combination of properties in the model (typically in the 'Wind' group) and properties of the fan object. Hot key: 8 (toggles wind)":
        "访问高级风功能。风由模型中的属性（通常在“风”组中）与风对象属性共同控制。快捷键：8（切换风）",
    "Add geometry to the model based on a set of predefined templates. Once a template has been added, select any of the new generators to edit the model.":
        "基于一组预定义模板向模型添加几何体。模板添加后，可选择任意新生成器来编辑模型。",
    "All property edits apply to the selected generator(s). This mode affects all of the individuals nodes created by the generator(s) and allows parent curve editing. Hot key: Tab":
        "所有属性编辑都作用于选定的生成器。此模式会影响该生成器创建的所有独立节点，并允许编辑父曲线。快捷键：Tab",
    "All property edits apply to the selected node(s). Property edits in this mode are offsets to the the value computed by the generator and should be used for fine tuning only. Hot key: Tab":
        "所有属性编辑都作用于选定的节点。此模式下的属性编辑是相对生成器计算值的偏移，仅用于微调。快捷键：Tab",
    "Click this button and then click any spot on the model to set the view's pivot point on that part of the model. The camera will zoom to and pivot around the selected point. Hot key: N ('double-clicking' anywhere on the model is a shortcut for this action)":
        "点击此按钮，再点击模型上任意位置，即可将视图的轴心点设置到该处。相机将缩放并以选定点为轴旋转。快捷键：N（在模型任意处“双击”可作为该操作的快捷方式）",
    "Compute ambient occlusion on the model. Once computed, ambient occlusion settings can be edited by selecting the light. Recomputing the tree will clear AO values. Hot key: O":
        "在模型上计算环境光遮蔽。计算完成后，可选择灯光来编辑环境光遮蔽设置。重新计算树会清除 AO 值。快捷键：O",
    "Control how much of the model is visible. Use these options to temporarily reduce computing and rendering complexity.":
        "控制模型可见量。使用这些选项可临时降低计算与渲染复杂度。",
    "Control which geometry types are shown in the viewport.": "控制视口中显示的几何类型。",
    "Eliminate colliding leaf meshes. Meshes are removed in the background according to the quality setting: High : leaf-leaf and leaf-branch collisions are eliminated, as many leaves as possible remain (slowest) Medium : leaf-leaf and leaf-branch collisions are eliminated, more leaves are removed in favor of faster execution Low : most leaf-leaf collision are eliminated, leaf-branch collisions are ignored None : collisions are ignored":
        "消除碰撞的叶片网格。将按质量设置在后台移除网格：高：消除叶-叶与叶-枝碰撞，尽可能保留叶片（最慢）；中：消除叶-叶与叶-枝碰撞，为加快执行而移除更多叶片；低：消除多数叶-叶碰撞，忽略叶-枝碰撞；无：忽略碰撞",
    "Ensure that the winding of each face matches the surface normals (ensures proper face culling in real-time rendering).":
        "确保每个面的环绕方向与表面法线一致（以保证实时渲染中的正确面剔除）。",
    "Export the model as a mesh (including animation, unwrapped textures, hierarchy, and other features if desired).":
        "将模型导出为网格（可包含动画、展开的纹理、层级及其他所需特性）。",
    "Flip the model around the specified axis. Often used to convert coordinate systems and align the mesh for use as a frond or leaf.":
        "绕指定轴翻转模型。常用于转换坐标系并对齐网格以用作叶或叶状体。",
    "For best results, use the \"Orientation\" and \"Flip\" options to orient the mesh based on its intended use as indicated by the figure.":
        "为获得最佳效果，请使用“方向”和“翻转”选项，按图中提示的预期用途来定向网格。",
    "Hold \"Space\" and click on an object to manually place a child with ID":
        "按住“空格”并点击对象以手动放置带 ID 的子项",
    "Make the models normals point in the opposite direction.": "让模型的法线指向相反方向。",
    "Reorient the mesh, if necessary. Often used to convert coordinate systems and align the mesh for use as a frond or leaf.":
        "如有需要，重新定向网格。常用于转换坐标系并对齐网格以用作叶或叶状体。",
    "Restrict growth based on the height of the parent mesh in the mesh's units.":
        "按父网格在其单位下的高度限制生长。",
    "Select a displacement map to edit. Displacement maps can be applied to branch geometry.":
        "选择要编辑的置换贴图。置换贴图可应用于枝条几何体。",
    "Select a mask to edit. Masks are applied in the 'Generation' group to control where children can grow.":
        "选择要编辑的遮罩。遮罩在“生成”组中应用，用于控制子项可以生长的位置。",
    "Select a material to edit. Materials are applied to geometry by dragging/dropping or selecting them from material properties.":
        "选择要编辑的材质。可通过拖放或从材质属性中选取来为几何体应用材质。",
    "Select a mesh to edit. Meshes are used as leaf, frond, and terrain geometry. They can also be used with force meshes to shape tree geometry or become part of the model.":
        "选择要编辑的网格。网格用作叶、叶状体和地形几何体，也可与力网格配合以塑造树木几何体或成为模型的一部分。",
    "Select the filename for the image.": "选择图像的文件名。",
    "Select the mask type (objects must pass all applied masks).": "选择遮罩类型（对象必须通过所有已应用的遮罩）。",
    "Select which channel of the image to use.": "选择要使用的图像通道。",
    "Select which tool is used for selected scene objects.": "选择用于所选场景对象的工具。",
    "Set options for the transform gizmo.": "设置变换 gizmo 的选项。",
    "Set the polygonal resolution for this model. Hot keys: D - decrease resolution, Ctrl+D increase resolution":
        "设置此模型的多边形分辨率。快捷键：D - 降低分辨率，Ctrl+D 提高分辨率",
    "Set the season of the current model. This is applicable to any models that makes use of the season properties on generators and the season curves on materials. Hot key: Alt+S":
        "设置当前模型的季节。适用于所有使用生成器季节属性与材质季节曲线的模型。快捷键：Alt+S",
    "Specify the mesh file and what processing should be applied to it.":
        "指定网格文件以及应对其应用的处理。",
    "Specify the style in which the scene is rendered.": "指定场景的渲染风格。",
    "Step 1: Hold \"Space\" and click in the scene to start a new branch":
        "第 1 步：按住“空格”并在场景中点击，开始新建枝条",
    "Step 2: Hold \"Space\" then click+drag the diamond at the end to trace out the branch's shape.":
        "第 2 步：按住“空格”，然后点击并拖动末端菱形以描绘枝条形状。",
    "Use the freehand tools to shape and edit individual nodes manually. Hot key: Tab":
        "使用徒手工具手动塑形和编辑各个节点。快捷键：Tab",
    "Zoom the camera view to show the selection. If nothing is selected, zoom out far enough to show the whole model. Hot key: Z":
        "缩放相机视图以显示选中内容。若未选任何内容，则缩小到足以显示整个模型。快捷键：Z",
}


def norm(s):
    return re.sub(r"\s+", " ", s).strip()


def main():
    src = os.path.join(NAT_TRANS, "tour_untranslated_zh.json")
    if not os.path.isfile(src):
        src = os.path.join(SRC_TRANS, "tour_untranslated_zh.json")
    tour = json.load(open(src, encoding="utf-8"))["translations"]
    by_norm = {norm(k): v for k, v in MAP.items()}

    out = {}
    if os.path.isfile(MAIN):
        try:
            d = json.load(open(MAIN, encoding="utf-8"))
            out = {k: v for k, v in d.get("translations", {}).items()
                   if v.strip() and v != "_skip_"}
        except Exception:
            pass
    skipped = []
    for k in tour:
        zh = by_norm.get(norm(k))
        if zh:
            out[k] = zh
        else:
            skipped.append(k)
    print("captured:", len(tour), " translated:", len(out), " skipped:", len(skipped))
    print("skipped examples:", skipped[:20])

    payload = {
        "$schema": "sp-translation-v1",
        "id": "speedtree-tour",
        "language": "zh-CN",
        "description": "运行时 UI 巡检采集并翻译的词条（含右键菜单/帮助文本）",
        "translations": out,
    }
    for d in (SRC_TRANS, NAT_TRANS):
        os.makedirs(d, exist_ok=True)
        p = os.path.join(d, "dictionary_zh.json")
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(payload, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
        print("wrote", p, len(out))


if __name__ == "__main__":
    main()
