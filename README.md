# SpeedTree 中文补丁

参考 **substance3d_chinese_translator**（Qt 显示层三层翻译 + sp-translation-v1 词库）
与 **Toolbag 汉化**（挂起启动 + DLL 注入 + 安装器 + .spm 关联）两套成熟方案，
为 SpeedTree Modeler 10（Qt 6.6）和 SpeedTree 9.x（Qt 5.12）做的运行时汉化补丁。

SpeedTree 没有 Python 插件接口，所以把 substance 的方案改成注入式原生引擎：
启动器把 Hook DLL 注入进程，Hook 在运行期做显示层翻译。不修改
`SpeedTree_Modeler.exe`，不改资源路径，也不碰模型/项目数据。

## 工作原理

```
SpeedTree_Modeler.exe（挂起启动）
   └─ 注入 SpeedTreeChineseHook.dll（链接 SpeedTree 的 Qt6）
         └─ 等 QApplication 就绪后：
            1. 加载 translations/*_zh.json（sp-translation-v1）
            2. 纯显示层翻译（不改控件/模型文本）：
               a. 列表/树/表格       -> QStyledItemDelegate::displayText
               b. QPainter::drawText -> IAT hook（绘制时替换文字）
               c. QFontMetrics/F 测量 -> IAT hook（sizeHint/省略号按中文宽度计算）
            3. 安装后刷新布局，让已有控件按中文字形重新计算尺寸
```

安装后的词库位于 `ChineseLauncher/translations/*_zh.json`，命中才替换，未命中保持英文。

## 目录结构

```text
SpeedTreeChinese/
├─ plugin/
│  ├─ cpp/
│  │  ├─ hook.cpp           原生翻译引擎（词库 + 纯绘制层翻译 + 字体测量 + 嗅探 + 中/英开关）
│  │  ├─ launcher.cpp       启动器（挂起启动 + 注入 + 恢复）
│  │  └─ installer.cpp      自包含安装器（GUI、提权、.spm 关联、快捷键、卸载）
│  ├─ native/               构建产物（Hook.dll、Launcher.exe、Installer.exe、translations 副本）
│  ├─ translations/         sp-translation-v1 词库（*_zh.json，8 个）
│  ├─ resources/            app_icon.rc + speedtree.ico（原版 SpeedTree 图标）
│  ├─ qt/                   Qt 6.5.3 SDK 的 ASCII 链接（指向 sp插件捆绑 SDK）
│  ├─ qt5-sdk/              Qt 5.12.0 最小构建 SDK（Qt5 Hook 编译用）
│  ├─ scripts/
│  │  ├─ build.bat          一键编译（Hook/Launcher/Installer，编入图标）
│  │  └─ embed_files.py     把 DLL/启动器/词库打进安装器（payload，原子替换）
│  └─ tools/
│     ├─ extract_ui_strings.py   exe/.spm 静态提取 UI 词条
│     ├─ build_dictionary.py     由提取清单生成词库骨架
│     ├─ manual/                人工翻译批次 JSON（manual_batch_*.json）
│     ├─ output/                工具输出文件
│     └─ scratch/               提取/分析临时文件（非发布内容）
├─ dist/
│  └─ SpeedTreeChineseInstaller.exe   发布安装器（含载荷）
└─ README.md
```

## 构建

前置：VS 2022 C++ 生成工具；Qt 6.5.3 SDK 以 `plugin\qt` ASCII 链接；
Qt 5.12.0 最小构建 SDK 已包含在 `plugin\qt5-sdk`（用于 SpeedTree 9.x / Qt5 版本）。

```bat
plugin\scripts\build.bat
python plugin\scripts\embed_files.py
```

产物：`dist\SpeedTreeChineseInstaller.exe`。

## 使用与卸载

1. 运行安装器（需管理员），选择含 `SpeedTree_Modeler.exe` 的 `win64` 目录。
2. 通过桌面/开始菜单“SpeedTree 中文版”启动，或双击 `.spm`（已关联）。
3. 卸载：再运行安装器，选同一目录后点“卸载”。

## 词库

sp-translation-v1 格式，放在 `translations/*_zh.json`：

```json
{
  "$schema": "sp-translation-v1",
  "id": "speedtree-zh",
  "language": "zh-CN",
  "description": "SpeedTree 中文界面",
  "translations": {
    "File": "文件",
    "Material": "材质"
  }
}
```

空译文案是占位符（引擎忽略）。可通过
`python plugin\scripts\build_dictionary.py` 从探针采集的英文清单生成待填骨架。
