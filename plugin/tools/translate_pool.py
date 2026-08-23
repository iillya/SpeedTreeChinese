#!/usr/bin/env python3
"""Auto translate the static SpeedTree string pool to Chinese.

Word/phrase glossary + composition rules.  Run after extract_complete.py.
Writes scratch/translated.json {english: chinese} for the untranslated pool and
prints a reviewable sample.
"""

import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
SCRATCH = os.path.join(HERE, "scratch")

GLOSSARY = {
    # geometry / mesh
    "Mesh": "网格", "Vertex": "顶点", "Vertices": "顶点", "Edge": "边", "Edges": "边",
    "Face": "面", "Faces": "面", "Polygon": "多边形", "Polygons": "多边形",
    "Triangle": "三角形", "TriangleCount": "三角形数", "Geometry": "几何体",
    "Curve": "曲线", "Profile": "截面", "Section": "截面", "Shape": "形状",
    "Contour": "轮廓", "Surface": "表面", "Silhouette": "轮廓", "Sphere": "球体",
    "Cube": "立方体", "Cylinder": "圆柱", "Conical": "圆锥", "Ellipsoid": "椭球",
    "Plane": "平面", "Box": "盒体", "Card": "卡片", "Target": "目标", "Bone": "骨骼",
    "Skeleton": "骨架", "Skin": "蒙皮", "Skinning": "蒙皮", "Weights": "权重",
    "Influence": "影响", "Hierarchy": "层级", "Parent": "父级", "Child": "子级",
    "Children": "子级", "Sibling": "同级", "Root": "根", "Tree": "树", "Node": "节点",
    "Instance": "实例", "Primitive": "图元", "Clustered": "簇状",
    # tree / vegetation
    "Branch": "枝条", "Branches": "枝条", "Branching": "分枝", "Leaf": "叶片",
    "Leaves": "叶片", "LeafCluster": "叶簇", "Frond": "叶", "Fronds": "叶",
    "Trunk": "主干", "Root": "根", "Roots": "根", "Spine": "棘", "Stirp": "茎",
    "LOD": "LOD", "Lods": "LOD", "Crown": "树冠", "Canopy": "树冠",
    "Wind": "风", "Windward": "迎风", "Growth": "生长", "Sprout": "芽",
    "Curl": "卷曲", "Freq": "频率", "Frequency": "频率", "Turbulence": "湍流",
    "Pull": "拉", "Reach": "伸度", "Length": "长度", "Radius": "半径",
    "RadiusScale": "半径缩放", "GrowthCurve": "生长曲线",
    # materials / textures
    "Material": "材质", "Materials": "材质", "Texture": "纹理", "Textures": "纹理",
    "Map": "贴图", "Maps": "贴图", "UV": "UV", "Atlas": "图集", "Tile": "贴片",
    "Tiles": "贴片", "Packing": "打包", "Blend": "混合", "Blending": "混合",
    "Alpha": "透明度", "Opacity": "不透明度", "Color": "颜色", "Colorize": "着色",
    "Ambient": "环境光", "Diffuse": "漫反射", "Specular": "镜面反射",
    "Bump": "凹凸", "Normal": "法线", "NormalMap": "法线贴图", "Gloss": "光泽",
    "Roughness": "粗糙度", "Metallic": "金属度", "Fresnel": "菲涅尔",
    "Mask": "遮罩", "Masks": "遮罩", "Channel": "通道", "Channels": "通道",
    "Swizzle": "通道互换", "Sampler": "采样器", "PackingMode": "打包模式",
    "Seed": "种子", "Random": "随机", "Variation": "变化", "Randomize": "随机化",
    "Salt": "盐", "Detail": "细节", "Base": "基础", "Pattern": "图案",
    # motion / transform
    "Rotation": "旋转", "Rotate": "旋转", "Scale": "缩放", "Translate": "平移",
    "Move": "移动", "Offset": "偏移", "Anchor": "锚点", "Align": "对齐",
    "Flip": "翻转", "Twist": "扭曲", "Bend": "弯曲", "Roll": "翻滚", "Pitch": "俯仰",
    "Yaw": "偏航", "Weld": "焊接", "Morph": "变形", "Transform": "变换",
    "Pivot": "轴心", "Origin": "原点", "Orientation": "方向", "Direction": "方向",
    "Vector": "向量", "Matrix": "矩阵", "Snap": "吸附", "Grid": "网格",
    "Constraint": "约束", "Axis": "轴", "Axes": "轴", "Position": "位置",
    "Placement": "放置", "Spacing": "间距", "Gap": "间隙", "Sink": "下沉",
    "Float": "浮动", "Interpolation": "插值", "Tween": "补间", "Inverse": "反向",
    "Mirror": "镜像", "Reflect": "反射", "Barrel": "桶形", "Extrusion": "挤出",
    "Inflation": "膨胀", "Deform": "变形", "Jitter": "抖动", "Cohesion": "内聚力",
    # UI verbs
    "Add": "添加", "Remove": "移除", "Delete": "删除", "Rename": "重命名",
    "Copy": "复制", "Cut": "剪切", "Paste": "粘贴", "Edit": "编辑", "Apply": "应用",
    "Reset": "重置", "Clear": "清除", "Save": "保存", "Open": "打开",
    "Import": "导入", "Export": "导出", "Generate": "生成", "Select": "选择",
    "Show": "显示", "Hide": "隐藏", "Enable": "启用", "Disable": "禁用",
    "Use": "使用", "Include": "包含", "Exclude": "排除", "Custom": "自定义",
    "Default": "默认", "Auto": "自动", "Manual": "手动", "Toggle": "切换",
    "Lock": "锁定", "Unlock": "解锁", "Compress": "压缩", "Convert": "转换",
    "Merge": "合并", "Split": "拆分", "Group": "分组", "Ungroup": "取消分组",
    "Duplicate": "复制", "Arrange": "排列", "Sort": "排序", "Build": "构建",
    "Preview": "预览", "Refresh": "刷新", "Clean": "清理", "Collapse": "折叠",
    "Expand": "展开", "Detach": "分离", "Pin": "固定", "Dock": "停靠",
    "Float": "浮动", "Close": "关闭", "Retain": "保留", "Allow": "允许",
    "Compare": "比较", "Reach": "伸度", "Smooth": "平滑", "Sharpen": "锐化",
    "Blend": "混合", "Compress": "压缩", "Flatten": "展平", "Extrude": "挤出",
    # scene / render
    "Scene": "场景", "Camera": "相机", "Cameras": "相机", "Light": "灯光",
    "Lights": "灯光", "Render": "渲染", "Shadow": "阴影", "Shadows": "阴影",
    "Shader": "着色器", "Environment": "环境", "Background": "背景",
    "Foreground": "前景", "Exposure": "曝光", "Framebuffer": "帧缓冲",
    "Motion": "运动", "Blur": "模糊", "Depth": "深度", "Of": "的", "Off": "关闭",
    "Filter": "滤镜", "Effects": "特效", "PostProcess": "后处理",
    # timing / animation
    "Time": "时间", "Timeline": "时间轴", "Animation": "动画", "Keyframe": "关键帧",
    "Frame": "帧", "Frames": "帧", "FPS": "帧率", "Rate": "速率", "Speed": "速度",
    "Duration": "时长", "Phase": "相位", "Cycle": "循环", "Loop": "循环",
    "Play": "播放", "Pause": "暂停", "Stop": "停止", "Rewind": "快退",
    "Starts": "起始", "Ends": "结束", "Before": "之前", "After": "之后",
    "Delay": "延迟", "Lifetime": "生命周期", "Age": "年龄", "Grow": "生长",
    # quality / numeric
    "High": "高", "Low": "低", "Max": "最大", "Min": "最小", "Best": "最佳",
    "Full": "完整", "Half": "一半", "Double": "双", "Average": "平均",
    "Current": "当前", "All": "全部", "None": "无", "Value": "值", "Values": "值",
    "Factor": "系数", "Scale": "缩放", "Amount": "量", "Count": "数量",
    "Number": "数字", "Level": "级别", "Size": "尺寸", "Width": "宽度",
    "Height": "高度", "Depth": "深度", "Offset": "偏移", "Angle": "角度",
    "Weight": "权重", "Density": "密度", "Volume": "体积", "Area": "面积",
    "Ratio": "比例", "Aspect": "纵横比", "Unit": "单位", "Units": "单位",
    "Meters": "米", "Centimeters": "厘米", "Pixels": "像素", "Pixel": "像素",
    "Percentage": "百分比", "Threshold": "阈值", "Limit": "限制", "Range": "范围",
    "Field": "字段", "Sample": "采样", "Sampling": "采样", "Resolution": "分辨率",
    "Byte": "字节", "Bytes": "字节", "Bit": "位", "Bits": "位", "Buffer": "缓冲",
    "Cache": "缓存", "Memory": "内存", "Index": "索引",
    # location / orientation
    "Left": "左", "Right": "右", "Top": "上", "Bottom": "下", "Center": "中心",
    "Middle": "中间", "Up": "上", "Down": "下", "Front": "前", "Back": "后",
    "Flex": "弯曲", "Horizontal": "水平", "Vertical": "垂直", "Forward": "向前",
    "Backward": "向后", "Inward": "向内", "Outward": "向外", "Global": "全局",
    "Local": "局部", "World": "世界", "Relative": "相对", "Near": "近", "Far": "远",
    # common nouns
    "Object": "对象", "Objects": "对象", "Model": "模型", "Asset": "资源",
    "Assets": "资源", "File": "文件", "Files": "文件", "Folder": "文件夹",
    "Path": "路径", "Info": "信息", "Information": "信息", "Options": "选项",
    "Option": "选项", "Settings": "设置", "Preferences": "首选项", "Properties": "属性",
    "Property": "属性", "Output": "输出", "Input": "输入", "Source": "源",
    "Data": "数据", "Format": "格式", "Extension": "扩展名", "Name": "名称",
    "Names": "名称", "Type": "类型", "Style": "样式", "Mode": "模式",
    "Item": "项目", "Items": "项目", "Slot": "槽位", "Content": "内容",
    "Layer": "层", "Layers": "层", "Level": "级别", "Group": "组", "Groups": "组",
    "List": "列表", "Tree": "树", "Panel": "面板", "Display": "显示",
    "Window": "窗口", "View": "视图", "Views": "视图", "Tool": "工具",
    "Tools": "工具", "Help": "帮助", "Edit": "编辑", "Viewport": "视口",
    "Selection": "选择", "Settings": "设置", "Information": "信息",
    "Favorites": "收藏夹", "Preset": "预设", "Presets": "预设", "Profile": "配置",
    "Converter": "转换器", "Helper": "辅助工具", "Exporter": "导出器",
    "Importer": "导入器", "Generator": "生成器", "Preview": "预览",
    # misc connectors / state
    "No": "无", "Not": "非", "Too": "过于", "And": "和", "Or": "或", "Of": "的",
    "For": "用于", "To": "到", "By": "按", "In": "中", "On": "开启", "At": "在",
    "From": "从", "With": "带", "Enable": "启用", "Disable": "禁用",
    "Use": "使用", "Game": "游戏", "Power": "电源", "Battery": "电池",
    "Contact": "接触", "Sensor": "传感器", "Simulation": "仿真",
    "Selected": "已选", "Adjusting": "调整", "New": "新建", "Existing": "已有",
    "External": "外部", "Externals": "外部", "Internal": "内部",
    "Vertices": "顶点", "Fragment": "片元", "Surface": "表面",
    "AlphaMap": "透明度贴图", "Occlusion": "遮蔽", "Culling": "剔除",
    "CullingProxies": "剔除代理", "Collision": "碰撞", "Colliders": "碰撞体",
    "Proxy": "代理", "Proxies": "代理", "Weathering": "风化", "Damage": "损伤",
    "Broken": "断裂", "Destroyed": "摧毁", "Nipped": "剪断",
    "Season": "季节", "Class": "类别", "Credits": "积分", "Coins": "金币",
    "Force": "力", "Forces": "力", "Gravity": "重力", "Friction": "摩擦",
    "Elasticity": "弹性", "Stiffness": "刚度", "Drag": "阻力", "Power": "强度",
    "Distribution": "分布", "Pattern": "图案", "Instancing": "实例化",
    "Billboard": "广告牌", "Billboards": "广告牌", "Impostor": "代理面片",
    "WindSpeed": "风速", "WindDir": "风向", "Strength": "强度",
    "Steps": "步数", "Stages": "阶段", "Ordering": "顺序", "Slots": "槽位",
    "Points": "点", "Envelope": "包络", "Wrap": "包裹", "Overwrap": "重叠包裹",
    "HiSpeed": "高速", "First": "第一", "Last": "最后", "Next": "下一个",
    "Previous": "上一个", "Main": "主", "Secondary": "次要", "Primary": "主要",
}

GLOSSARY_EXTRA = {
    "Update": "更新", "Bend": "弯曲", "Pruned": "修剪", "Removed": "移除",
    "Stopped": "停止", "Solution": "解", "Bent": "弯曲", "Large": "过大",
    "Short": "过短", "Space": "空间", "Information": "信息", "Line": "行",
    "Script": "脚本", "Pivot": "轴心", "Straighten": "拉直", "Simple": "简单",
    "Block": "块", "Info": "信息", "Reached": "达到", "Image": "图像",
    "Movement": "运动", "Distance": "距离", "Destination": "目标点",
    "Double": "双", "Hyphen": "连字符", "Comment": "注释", "Names": "名称",
    "Found": "找到", "Restrictions": "限制", "Include": "包含", "Exposure": "曝光",
    "Opened": "已打开", "Change": "更改", "Date": "日期", "Does": "未",
    "Exist": "存在", "Corrupted": "已损坏", "Created": "已创建", "Reading": "读取",
    "Writing": "写入", "Converted": "已转换", "Key": "键", "Changed": "已更改",
    "Filter": "滤镜", "Order": "顺序", "Bitstream": "比特流", "Slices": "切片",
    "Center": "中心", "Weights": "权重", "Levels": "级别", "Compute": "计算",
    "Flags": "标记", "Limit": "限制", "Reached": "达到", "Output": "输出",
    "Multiplier": "乘数", "Threshold": "阈值", "Override": "覆盖",
    "Constraints": "约束", "Maximize": "最大化", "Minimize": "最小化",
    "Restore": "还原", "Reorder": "重排", "Straight": "笔直", "Bendy": "可弯",
    "Intersect": "相交", "Overlap": "重叠", "Tangent": "切线", "Bitangent": "副切线",
    "Derivative": "导数", "Gradient": "渐变", "Sharpness": "锐度", "Brightness": "亮度",
    "Contrast": "对比度", "Saturation": "饱和度", "Hue": "色相", "Gamma": "伽马",
    "Swizzle": "通道互换", "Mip": "Mip", "Mips": "Mip", "Filtering": "过滤",
    "Bilinear": "双线性", "Trilinear": "三线性", "Anisotropic": "各向异性",
    "Wrap": "包裹", "Clamp": "钳制", "Mirror": "镜像", "Tiling": "平铺",
    "Signed": "有符号", "Unsigned": "无符号", "Normalized": "归一化",
    "Uniform": "均匀", "Constant": "恒定", "Linear": "线性", "Cubic": "三次",
    "Quadratic": "二次", "Smoothstep": "平滑步进", "Clamped": "钳制",
    "Culling": "剔除", "Backface": "背面", "Frontface": "正面",
    "Transparent": "透明", "Opaque": "不透明", "Cutout": "镂空", "Blended": "混合",
    "Additive": "叠加", "Multiplicative": "相乘", "Subtractive": "相减",
    "Screen": "屏幕", "Overlay": "叠加", "Multiply": "相乘", "Divide": "相除",
    "Pow": "幂", "Sin": "正弦", "Cos": "余弦", "Abs": "绝对值", "Clamp": "钳制",
    "Lerp": "插值", "Saturate": "饱和", "Ceil": "向上取整", "Floor": "向下取整",
    "Round": "四舍五入", "Sign": "符号", "Viewport": "视口", "Foliage": "植被",
    "Grass": "草", "Sapling": "幼苗", "Seedling": "树苗", "Bush": "灌木",
    "Shrub": "灌木", "Clump": "丛", "Cluster": "簇", "Wisp": "细枝",
    "Frond": "叶", "Fronds": "叶", "Strow": "散铺", "Skirt": "裙边",
    "Ribbon": "条带", "Torn": "撕裂", "Lacerated": "撕裂", "Split": "分裂",
    "Sloped": "倾斜", "Emerge": "伸出", "Flush": "齐平", "Revolt": "反绕",
    "Coil": "卷绕", "Furl": "卷曲", "Quiver": "颤动", "Slump": "下垂",
    "Droop": "下垂", "Lean": "倾斜", "Gust": "阵风", "Gale": "强风",
    "Storm": "风暴", "Breeze": "微风", "Tumble": "翻滚", "Twirl": "旋转",
    "Sway": "摇摆", "Shiver": "发抖", "Waver": "摇摆", "Shake": "抖动",
    "Ripple": "涟漪", "Wave": "波浪", "Trail": "拖尾", "Streak": "条纹",
    "Wispy": "细絮", "Fuzzy": "毛糙", "Hairy": "多毛", "Sharp": "尖锐",
    "Brittle": "脆", "Flexible": "柔韧", "Rigid": "刚性", "Solid": "实体",
    "Hollow": "空心", "Walled": "有壁", "Tube": "管", "Capsule": "胶囊",
    "Spike": "尖刺", "Bract": "苞片", "Burl": "树瘤", "Knot": "节疤",
    "Crack": "裂纹", "Split": "裂缝", "Fissure": "裂隙", "Rift": "裂缝",
}
GLOSSARY.update(GLOSSARY_EXTRA)


# order matters: exact multi-word phrases are matched first
PHRASES = {
    "Add Image": "添加图像",
    "Adjust frond start": "调整叶片起点",
    "Advanced Options": "高级选项",
    "Alpha Is Opacity": "透明度即不透明度",
    "Alpha test and blending": "透明度测试与混合",
    "All (all views)": "全部（所有视图）",
    "Ambient Factor": "环境光系数",
    "Aspect Height": "纵横比高度",
    "Aspect Width": "纵横比宽度",
    "Background Color": "背景颜色",
    "Background Alpha": "背景透明度",
    "Base Layer": "基础层",
    "Best Quality Scale": "最佳质量缩放",
    "Billboard Count": "广告牌数量",
    "Billboard atlas resolution": "广告牌图集分辨率",
    "Billboard vertex packing": "广告牌顶点打包",
    "Blend Shape": "混合形状",
    "Blend Weight": "混合权重",
    "Bone Count": "骨骼数量",
    "Bottom blend range": "底部混合范围",
    "Branch Rate": "分枝速率",
    "Bump Factor": "凹凸系数",
    "Bump Map (NormalMap)": "凹凸贴图（法线贴图）",
    "Byte Count": "字节数",
    "Camera Rate": "相机速率",
    "Capture Frame Rate": "采集帧率",
    "Cell Width": "单元格宽度",
    "Clear Shader Cache": "清除着色器缓存",
    "Clear existing objects": "清除已有对象",
    "Close Others": "关闭其他",
    "Close Other Groups": "关闭其他组",
    "Close Group": "关闭组",
    "Cohesion Offset": "内聚力偏移",
    "Cohesion Scale": "内聚力缩放",
    "Color Adjustment Amount": "颜色调整量",
    "Color Channels": "颜色通道",
    "Compress Level": "压缩级别",
    "Compute Scale": "计算缩放",
    "Constraint Count": "约束数量",
    "Constraint Group": "约束组",
    "Contact Size": "接触尺寸",
    "Content Count": "内容数量",
    "Convert Animation Curves At": "在…处转换动画曲线",
    "Copy adjustments": "复制调整",
    "Current Level": "当前级别",
    "Current Preset": "当前预设",
    "Custom Width": "自定义宽度",
    "Default Crop Size": "默认裁剪尺寸",
    "Default Scale": "默认缩放",
    "Delete current layer": "删除当前层",
    "Delete preset": "删除预设",
    "Detailed Info": "详细信息",
    "Device Count": "设备数量",
    "Diffuse Color": "漫反射颜色",
    "Diffuse Factor": "漫反射系数",
    "Destination Count": "目标数量",
    "Edit Options": "编辑选项",
    "Frame Rate": "帧率",
    "From Geometry": "来自几何体",
    "Full Quality": "完整质量",
    "Game Conversion": "游戏转换",
    "Game Options": "游戏选项",
    "Global Scale": "全局缩放",
    "Hierarchy Selection": "层级选择",
    "Ignored by Wind": "不受风影响",
    "Leaf Texture": "叶片纹理",
    "Material Packing": "材质打包",
    "Mesh Compression": "网格压缩",
    "Modeler Options": "模型器选项",
    "Motion Blur": "运动模糊",
    "New Branch": "新建枝条",
    "Object Properties": "对象属性",
    "Overwrite Existing": "覆盖已有",
    "Parent Branch": "父枝条",
    "Preset Browser": "预设浏览器",
    "Profile Options": "配置选项",
    "Rotate Around": "围绕旋转",
    "Rotation Snap": "旋转吸附",
    "Sampling Rate": "采样率",
    "Smooth Shading": "平滑着色",
    "Texture Size": "纹理尺寸",
    "Time Line": "时间轴",
    "Update Existing": "更新已有",
    "Use Wind": "使用风",
    "Vertex Color": "顶点颜色",
    "Wind Direction": "风向",
    "Wind Speed": "风速",
    "Low Quality": "低质量",
    "High Quality": "高质量",
}

PHRASES_EXTRA = {
    "Add and update animation": "添加并更新动画",
    "Additional Scale Factor": "附加缩放系数",
    "Adjust icon size": "调整图标大小",
    "All licenses in use": "所有许可证已用完",
    "Anchor index out of bounds": "锚点索引越界",
    "Aspect ratio restricted": "纵横比受限",
    "Auto-generate points options": "自动生成点选项",
    "Average time": "平均时间",
    "Back Plane Rotation": "背面平面旋转",
    "Bad Polygon Size": "多边形尺寸异常",
    "Baseline Noise": "基线噪点",
    "Battery Level": "电量",
    "Before bend edits": "弯曲编辑之前",
    "Black Level": "黑电平",
    "Branch pruned by geometry force (no solution)": "被几何力剪枝（无解）",
    "Branch pruned by geometry force (too bent)": "被几何力剪枝（过于弯曲）",
    "Branch removed by geometry force (no solution)": "被几何力移除（无解）",
    "Branch removed by geometry force (too bent)": "被几何力移除（过于弯曲）",
    "Branch stopped by geometry force (no solution)": "被几何力停止（无解）",
    "Branch stopped by geometry force (too bent)": "被几何力停止（过于弯曲）",
    "Buffer too large for write": "缓冲区过大，无法写入",
    "Color space information": "色彩空间信息",
    "Copy edit line for script": "复制脚本编辑行",
    "Copy name for script": "复制脚本名称",
    "Copy pivot from High": "从高复制轴心",
    "Copy simple block for script": "复制简单脚本块",
    "Copy straighten from High": "从高复制拉直",
    "Custom info": "自定义信息",
    "Direction of image": "图像方向",
    "Direction of movement": "运动方向",
    "Direction offset": "方向偏移",
    "Distance to destination": "到目标点的距离",
    "Double hyphen within comment": "注释中的双连字符",
    "Duplicate names found in": "中发现重复名称",
    "Export restrictions include": "导出限制包括",
    "Export selected as mesh...": "将选中项导出为网格…",
    "Exposure index": "曝光指数",
    "Field not opened": "字段未打开",
    "Field of view": "视场",
    "File change date and time": "文件修改日期与时间",
    "File does not exist": "文件不存在",
    "File format of image file": "图像文件格式",
    "File is corrupted": "文件已损坏",
    "File is not open": "文件未打开",
    "File is open": "文件已打开",
    "File not created": "文件未创建",
    "File not open for reading": "文件未以读取方式打开",
    "File not open for writing": "文件未以写入方式打开",
    "File not opened": "文件未打开",
    "File too large": "文件过大",
    "File too short": "文件过短",
    "File units converted to": "文件单位转换为",
    "Frond area threshold": "叶片面积阈值",
    "Generate slices with center line": "沿中心线生成切片",
    "Generator wind weights and levels": "生成器风力权重与级别",
}
PHRASES.update(PHRASES_EXTRA)

GLOSSARY_EXTRA2 = {
    "Boolean": "布尔", "Compose": "组合", "Connect": "连接", "Euler": "欧拉",
    "Fade": "淡出", "Gain": "增益", "Generation": "生成", "Editor": "编辑器",
    "Ignore": "忽略", "Individual": "单个", "Integer": "整数", "Interpolate": "插值",
    "Invert": "反转", "Fold": "折叠", "FoldOut": "展开", "Facing": "朝向",
    "Smooth": "平滑", "Spread": "铺散", "LightList": "灯光列表", "LOD": "LOD",
    "Cache": "缓存", "Connectivity": "连通性", "ConnectivityList": "连通列表",
    "Trim": "修剪", "Taper": "锥化", "Shag": "蓬松", "Shagginess": "蓬松度",
    "Turbulence": "湍流", "Vibration": "振动", "Oscillate": "振荡",
    "Oscillation": "振荡", "Sway": "摇摆", "Windy": "多风", "Choppy": "起伏",
    "Reach": "伸度", "Permanence": "持久", "Length": "长度", "Radius": "半径",
    "Envelope": "包络", "Bulge": "隆起", "Belly": "鼓腹", "Pinch": "收窄",
    "Flatten": "展平", "Roundness": "圆度", "Squareness": "方形度",
    "EdgeFalloff": "边缘衰减", "Layering": "分层", "Spacing": "间距",
    "Horizontal": "水平", "Vertical": "垂直", "Clumping": "成簇",
    "Fidelity": "保真度", "Segments": "分段", "Segment": "分段",
    "Nodes": "节点", "Bud": "芽", "Sections": "截面", "Generations": "代",
    "Force": "力", "Forces": "力", "Influence": "影响", "Field": "场",
    "Matrix": "矩阵", "Vector": "向量", "Quaternion": "四元数",
    "Interpolation": "插值", "Bezier": "贝塞尔", "Hermite": "埃尔米特",
    "CatmullRom": "Catmull-Rom", "Ease": "缓动", "Bounce": "回弹",
}
GLOSSARY.update(GLOSSARY_EXTRA2)

PHRASES_EXTRA2 = {
    "As Optical Segments": "作为光学分段",
    "Boolean": "布尔",
    "Facing leaves": "朝向叶片",
    "Gain control": "增益控制",
    "Generation Editor": "生成编辑器",
    "Ignore lightList cache": "忽略灯光列表缓存",
    "Ignore parent": "忽略父级",
    "Ignore smooth LOD": "忽略平滑 LOD",
    "Ignore spread": "忽略铺散",
    "Integer overflow": "整数溢出",
    "Integer overflow in gtStripContig": "gtStripContig 整数溢出",
    "Integer overflow in gtStripSeparate": "gtStripSeparate 整数溢出",
    "Connectivity List": "连通性列表",
}
PHRASES.update(PHRASES_EXTRA2)

GLOSSARY_EXTRA3 = {
    "Enabled": "已启用", "Group": "组", "Label": "标签", "Lit": "受光",
    "Look": "注视", "Focal": "焦点", "Point": "点", "Make": "制作",
    "Metadata": "元数据", "Normalize": "归一化", "Padding": "内边距",
    "Paint": "绘制", "Pipeline": "管线", "Question": "问题", "Scaling": "缩放",
    "Single": "单个", "Chain": "链", "IK": "IK", "Skip": "跳过", "Save": "保存",
    "SDK": "SDK", "Areas": "区域", "Spheric": "球形", "Standard": "标准",
    "Status": "状态", "Stream": "流", "Sum": "总和", "Translation": "平移",
    "Correction": "校正", "Shift": "位移", "Tolerance": "容差", "Variant": "变体",
    "Selections": "选择", "Yes": "是", "Warnings": "警告", "Style": "样式",
    "Set": "设置", "Creator": "创建器", "Saved": "已保存", "Save": "保存",
    "Atmospheres": "大气", "Negative": "负", "Illegal": "非法",
    "Policy": "策略", "Qualifier": "限定符", "User": "用户", "Notice": "通知",
    "Coincident": "重合", "Float": "浮动", "Number": "编号", "Null": "空",
    "Value": "值", "Count": "数量", "Size": "尺寸", "Offset": "偏移",
    "Relative": "相对", "Tile": "贴片", "Lock": "锁定", "Correction": "校正",
    "Correction": "校正", "Tolerance": "容差",
}
GLOSSARY.update(GLOSSARY_EXTRA3)

PHRASES_EXTRA3 = {
    "LOD enabled": "LOD 已启用",
    "LOD group": "LOD 组",
    "Look at": "注视",
    "Look at focal point": "注视焦点",
    "Make UV area": "制作 UV 区域",
    "Negative value is illegal": "负值非法",
    "Padding value": "内边距值",
    "Paint Displacement": "绘制置换",
    "Policy Qualifier User Notice": "策略限定符用户通知",
    "Scaling UV": "缩放 UV",
    "Single Chain IK": "单链 IK",
    "Skip texture save": "跳过纹理保存",
    "SpeedTree Pipeline SDK": "SpeedTree Pipeline SDK",
    "SpeedTree UV Areas": "SpeedTree UV 区域",
    "Spheric XYZ": "球形 XYZ",
    "Standard atmospheres": "标准大气",
    "Stream too short": "流过短",
    "Translation UV": "平移 UV",
    "UV style": "UV 样式",
    "UV tile": "UV 贴片",
    "UV warnings": "UV 警告",
    "Variant selections": "变体选择",
}
PHRASES.update(PHRASES_EXTRA3)


def tr_word(w):
    r = GLOSSARY.get(w)
    if r is None:
        r = GLOSSARY.get(w.capitalize())
    if r is None:
        r = GLOSSARY.get(w.title())
    return r


def translate(text):
    t = text.strip()
    if not t:
        return ""
    if t in PHRASES:
        return PHRASES[t]
    # mnemonic ampersand preserved separately
    mnemonic = ""
    body = t
    if "&" in body:
        mnemonic = "&"
        body = body.replace("&", "")
    ellip = ""
    if body.endswith("..."):
        ellip = "…"
        body = body[:-3]
    # parenthetical qualifier
    m = re.match(r"^(.*?)\s*\((.*)\)\s*$", body)
    paren = ""
    if m:
        body = m.group(1).strip()
        paren = "（" + translate(m.group(2).strip()) + "）"
    # "No/not X" / "X and Y" / "X of Y"
    low = body
    words = [w for w in re.split(r"[^A-Za-z]+", body) if w]
    if not words:
        return t
    out_words = []
    for w in words:
        # U/V/W/X/Y/Z are axis labels; keep them verbatim
        if w in ("U", "V", "W", "X", "Y", "Z", "UV", "XYZ", "LOD", "IK", "SDK"):
            out_words.append(w)
        else:
            out_words.append(GLOSSARY.get(w, tr_word(w) or w))
    # join: mostly space-joined; avoid duplicating spaces
    joined = " ".join(out_words)
    joined = joined.replace("  ", " ")
    return mnemonic + joined + paren + ellip


def main():
    path = os.path.join(SCRATCH, "complete_untranslated.txt")
    lines = [x.strip() for x in open(path, encoding="utf-8") if x.strip()]
    out = {}
    skipped = []
    for s in lines:
        zh = translate(s)
        out[s] = zh
        if zh == s:
            skipped.append(s)
    with open(os.path.join(SCRATCH, "translated.json"), "w", encoding="utf-8") as fh:
        json.dump(out, fh, ensure_ascii=False, indent=1)
        fh.write("\n")
    print("translated", len(out), " skipped/untouched", len(skipped))
    for k, v in list(out.items())[:60]:
        print(repr(k), "->", repr(v))


if __name__ == "__main__":
    main()
