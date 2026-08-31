# SpeedTree 中文补丁

当前仅提供 **Inno Setup 安装器**，文件名为 `SpeedTreeChineseInstaller.exe`。安装、更新、迁移及卸载说明见 [安装指南](docs/installer.md)。卸载请使用 Windows“已安装的应用”，不再使用旧安装器。

把 **SpeedTree Modeler** 的界面汉化成中文的小工具。它只在 SpeedTree 运行时，于**显示层**把界面文字替换成中文，并自动按中文字形调整界面尺寸，让中文显示得更自然、不裁字。

它**不修改 SpeedTree 的任何原始文件**：不改 `SpeedTree_Modeler.exe`、不碰模型与项目数据（`.spm`），也不需要 SpeedTree 提供插件接口。

---

## 特性

- **全界面汉化**：菜单、工具栏、面板、对话框、节点参数等。
- **不染指原文件**：运行时注入，纯显示层替换，保留工程数据。
- **中/英一键切换**：随时切回英文。
- **中文宽度自适应**：界面尺寸按中文文字宽度调整，中文不裁字。
- **词库可自定义**：词典是纯文本 JSON，随时可增删词条。

---

## 支持版本

- **SpeedTree Modeler 10.x**（Qt 6.6，如 10.2.0）
- **SpeedTree Modeler 9.x**（Qt 5.12）

安装器会自动识别 SpeedTree 使用的是 Qt5 还是 Qt6，并加载对应的汉化引擎，无需手动选择。

---

## 安装

1. 运行 **`SpeedTreeChineseInstaller.exe`**（需要管理员权限）。
2. 选择 SpeedTree 的安装目录，即含有 `SpeedTree_Modeler.exe` 的 `win64` 文件夹。通常默认就是：

   ```
   C:\Program Files\SpeedTree\SpeedTree Modeler v10.2.0\win64
   ```

3. 按向导点击“安装”。

安装完成后，程序会：

- 把汉化组件安装到该目录下的 `ChineseLauncher\` 文件夹；
- 在**桌面**和**开始菜单**创建「**SpeedTree 中文版**」快捷方式；
- 按选项接管已验证的 `.spm` 打开命令，不强制更改 Windows 默认应用选择。

---

## 使用

- 双击桌面或开始菜单的「**SpeedTree 中文版**」，即可启动汉化后的 SpeedTree。
- 也可以直接双击任意 `.spm` 文件，会由汉化版打开。

### 切换中/英

- 按快捷键 **F3**。
- 或点击 SpeedTree 菜单栏的「**中/英**」项。
- 在菜单栏「中/英」上**点右键**，点击录入按钮后按新键，再点“确定”保存，下次启动继续使用。默认 F3；“重置”也需要点击“确定”提交，取消或关闭不保存。
- 快捷键独立保存在 `%LOCALAPPDATA%\SpeedTreeChinese\hotkeys.json`，不修改宿主快捷键或工程。支持单键，不支持组合键；建议使用不与宿主操作冲突的功能键。

---

## 自定义词库

词库是 `sp-translation-v1` 格式的 JSON 文件，位于安装目录：

```
<SpeedTree 安装目录>\win64\ChineseLauncher\translations\*.zh.json
```

每个词条是一组「英文原文 → 中文」的映射，形如：

```json
{
  "translations": {
    "File": "文件",
    "Material": "材质"
  }
}
```

- 直接编辑该文件、增删词条，重新启动 SpeedTree 即可生效。
- 某个词条的值留空（`""`）表示占位符，引擎会忽略它、按原文显示。

---

## 卸载

保存工程并退出 SpeedTree，在 Windows“已安装的应用”中卸载“SpeedTree 中文补丁”。用户修改过的词典及额外文件保留，补丁生成的备份和快捷键配置清除；仅恢复仍属于补丁的工程打开命令。

## 常见问题

**启动后界面还是英文？**

先关闭所有 SpeedTree 窗口，确认是通过「**SpeedTree 中文版**」快捷方式启动的，而不是直接打开 `SpeedTree_Modeler.exe`。

**双击 `.spm` 没有走汉化版？**

确认安装时选择了工程关联选项；或在 `.spm` 上右键 →「打开方式」→ 选择「SpeedTree 中文版」。

**想完全还原、换回原界面？**

通过 Windows“已安装的应用”卸载；保留用户修改的词典，清除补丁生成的备份。

---

## 项目

菜单栏中的「**Bilibili 神说要凑数汉化**」与「**GitHub 仓库**」提供了作者署名、源码与更多信息。
