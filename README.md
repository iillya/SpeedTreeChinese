# SpeedTree 中文补丁

把 **SpeedTree Modeler** 的界面汉化成中文的小工具。它只在 SpeedTree 运行时，于**显示层**把界面文字替换成中文，并自动按中文字形调整界面尺寸，让中文显示得更自然、不裁字。

它**不修改 SpeedTree 的任何原始文件**：不改 `SpeedTree_Modeler.exe`、不碰模型与项目数据（`.spm`），也不需要 SpeedTree 提供插件接口。

---

## 特性

- **全界面汉化**：菜单、工具栏、面板、对话框、节点参数等。
- **不染指原文件**：运行时注入，纯显示层替换，零副作用。
- **中 / 英一键切换**：随时切回英文。
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

3. 点击 **「安装汉化」**。

安装完成后，程序会：

- 把汉化组件安装到该目录下的 `ChineseLauncher\` 文件夹；
- 在**桌面**和**开始菜单**创建「**SpeedTree 中文版**」快捷方式；
- 把 `.spm` 文件关联到「SpeedTree 中文版」，之后**双击 `.spm` 即可直接用汉化版打开**。

---

## 使用

- 双击桌面或开始菜单的「**SpeedTree 中文版**」，即可启动汉化后的 SpeedTree。
- 也可以直接双击任意 `.spm` 文件，会由汉化版打开。

### 切换中 / 英

- 按快捷键 **F3**。
- 或点击 SpeedTree 菜单栏的「**中 / 英**」项。
- 在菜单栏「中 / 英」上**点右键**，可以自定义切换快捷键（弹窗中「确定」保存、「重置」恢复默认 F3）。

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

再次运行 **`SpeedTreeChineseInstaller.exe`**，选择同一个 SpeedTree 目录，点击 **「卸载汉化」** 即可。卸载会：

- 删除汉化组件（`ChineseLauncher\` 目录）；
- 移除「SpeedTree 中文版」快捷方式；
- 恢复 `.spm` 文件的原始关联；
- **不触碰任何 SpeedTree 原始文件**。

---

## 常见问题

**启动后界面还是英文？**

先关闭所有 SpeedTree 窗口，确认是通过「**SpeedTree 中文版**」快捷方式启动的，而不是直接打开 `SpeedTree_Modeler.exe`。

**双击 `.spm` 没有走汉化版？**

重新运行安装器再点一次「安装汉化」即可重新关联；或在 `.spm` 上右键 →「打开方式」→ 选择「SpeedTree 中文版」。

**想完全还原、换回原界面？**

按「卸载」即可彻底移除补丁，SpeedTree 恢复原样。

---

## 项目

菜单栏中的「**Bilibili神说要凑数汉化**」与「**Github仓库**」提供了作者署名、源码与更多信息。
