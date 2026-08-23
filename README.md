# SpeedTree 中文补丁

参考 **substance3d_chinese_translator**（Qt 显示层三层翻译 + sp-translation-v1 词库）
与 **Toolbag 汉化**（挂起启动 + DLL 注入 + 安装器 + .spm 关联）两套成熟方案，
为 SpeedTree Modeler 10 做的运行时汉化补丁。

SpeedTree 没有 Python 插件接口，所以把 substance 的方案改成注入式原生引擎：
启动器把 Hook DLL 注入进程，Hook 在运行期做显示层翻译。不修改
`SpeedTree_Modeler.exe`，不改资源路径，也不碰模型/项目数据。

## 工作原理

```
SpeedTree_Modeler.exe（挂起启动）
   └─ 注入 SpeedTreeChineseHook.dll（链接 SpeedTree 的 Qt6）
         └─ 等 QApplication 就绪后：
            1. 加载 translations/*_zh.json（sp-translation-v1）
            2. 三层显示层翻译：
               a. QLabel/按钮/菜单 -> setText
               b. 列表/树/表格      -> QStyledItemDelegate::displayText
               c. QPainter::drawText -> IAT hook（自绘控件，如属性参数行）
            3. 每 2 秒重新应用（SpeedTree 加载文档后会重建控件）
```

词库：`translations/*_zh.json`，命中才替换，未命中保持英文。

## 目录结构

```text
plugin/
├─ cpp/                  源码
│  ├─ hook.cpp           原生翻译引擎（词库 + 三层显示翻译 + drawText IAT）
│  ├─ launcher.cpp       启动器（挂起启动 + 注入 + 恢复）
│  └─ installer.cpp      自包含安装器（GUI、提权、.spm 关联、快捷键、卸载）
├─ native/               构建产物（Hook.dll、Launcher、Installer）
├─ translations/         sp-translation-v1 词库（*_zh.json）
├─ resources/            安装器对话框模板（dialog.rc）
├─ qt/                   Qt 6.5.3 SDK 的 ASCII 链接（指向 sp插件捆绑 SDK）
└─ scripts/
   ├─ build.bat          一键编译（Hook/Launcher/Installer）
   ├─ embed_files.py     把 DLL/启动器/词库打进安装器（payload）
   └─ build_dictionary.py 从英文清单生成待填词库骨架
dist/
└─ SpeedTreeChineseInstaller.exe   发布安装器
```

## 构建

前置：VS 2022 C++ 生成工具；Qt 6.5.3 SDK 以 `plugin\qt` ASCII 链接。

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
