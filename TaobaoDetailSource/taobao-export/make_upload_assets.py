# -*- coding: utf-8 -*-
"""Generate Taobao upload assets: square main images + a 750px detail long strip."""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.abspath(__file__))
IMGS = os.path.join(ROOT, "detail-images")
SRC = os.path.splitext(__file__)[0]

FONT = r"C:\Windows\Fonts\NotoSansSC-VF.ttf"
FONT_BOLD = FONT  # variable font; we fake bold via size + maybe not supported

OUT = ROOT
os.makedirs(OUT, exist_ok=True)

BG = (18, 22, 28)
ACCENT = (80, 200, 120)
TEXT = (232, 234, 238)
MUTED = (170, 176, 184)
CARD = (34, 40, 48)


def font(size):
    return ImageFont.truetype(FONT, size)


def draw_text(draw, xy, text, size, fill=TEXT, anchor="la", max_w=None):
    f = font(size)
    if max_w is None:
        draw.text(xy, text, font=f, fill=fill, anchor=anchor)
        return f.getbbox(text)
    # manual wrap
    words = list(text)
    lines = []
    cur = ""
    for ch in words:
        t = cur + ch
        if draw.textlength(t, font=f) <= max_w:
            cur = t
        else:
            if cur:
                lines.append(cur)
            cur = ch
    if cur:
        lines.append(cur)
    x, y = xy
    for i, ln in enumerate(lines):
        draw.text((x, y + i * (size + 6)), ln, font=f, fill=fill, anchor="la")
    return f.getbbox(text)


def fit_square(path, size=800):
    im = Image.open(path).convert("RGB")
    canvas = Image.new("RGB", (size, size), BG)
    scale = min(size / im.width, size / im.height)
    nw, nh = round(im.width * scale), round(im.height * scale)
    im = im.resize((nw, nh), Image.LANCZOS)
    canvas.paste(im, ((size - nw) // 2, (size - nh) // 2))
    return canvas


# ---- main images 1-3 from existing detail crops ----
for i, n in enumerate(["01-overview", "02-panel-params", "03-panel-menu"], start=1):
    c = fit_square(os.path.join(IMGS, n + ".png"))
    c.save(os.path.join(OUT, f"主图-{i}.png"), "PNG", optimize=True)

# ---- main image 4: selling-points card ----
def main_card():
    size = 800
    img = Image.new("RGB", (size, size), BG)
    d = ImageDraw.Draw(img)
    # accent bar
    d.rounded_rectangle([0, 0, size, size], 0, fill=BG)
    d.rectangle([0, 0, size, 14], fill=ACCENT)
    x = 70
    y = 80
    d.text((x, y), "SPEEDTREE MODELER", font=font(34), fill=MUTED, anchor="la")
    y += 58
    d.text((x, y), "中文补丁", font=font(92), fill=TEXT, anchor="la")
    y += 130
    d.text((x, y), "让创作回到创作本身", font=font(40), fill=ACCENT, anchor="la")
    y += 96
    d.line([(x, y), (size - x, y)], fill=(60, 66, 74), width=2)
    y += 40
    items = [
        "纯显示层汉化，不改原文件",
        "覆盖菜单 / 参数 / 对话框",
        "F3 一键中英切换",
        "自动适配 9.x / 10.x",
    ]
    for it in items:
        d.ellipse([x, y + 26, x + 22, y + 48], fill=ACCENT)
        d.text((x + 38, y + 12), it, font=font(34), fill=TEXT, anchor="la")
        y += 78
    return img


main_card().save(os.path.join(OUT, "主图-4.png"), "PNG", optimize=True)


# ---- detail long strip (750 wide) ----
def detail_strip():
    W = 750
    blocks = []
    header_h = 360
    feature_h = 480
    cap_h = 80
    foot_h = 180

    imgs = [os.path.join(IMGS, n + ".png") for n in
            ["01-overview", "02-panel-params", "03-panel-menu"]]
    pics = []
    for p in imgs:
        im = Image.open(p).convert("RGB")
        if im.width > W:
            h = round(im.height * W / im.width)
            im = im.resize((W, h), Image.LANCZOS)
        pics.append(im)

    H = header_h + feature_h + sum(p.height + cap_h for p in pics) + foot_h
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    y = 0

    # header
    d.rectangle([0, y, W, y + header_h], fill=(20, 26, 33))
    d.rectangle([0, y, W, y + 10], fill=ACCENT)
    y += 70
    d.text((48, y), "SPEEDTREE MODELER 中文补丁", font=font(26), fill=MUTED, anchor="la")
    y += 52
    d.text((48, y), "让 SpeedTree 创作", font=font(58), fill=TEXT, anchor="la")
    y += 74
    d.text((48, y), "回到创作本身", font=font(58), fill=ACCENT, anchor="la")
    y += 96
    def pill_row(yy):
        x = 48
        for t in ["9.x / 10.x", "Qt5 / Qt6", "Windows"]:
            w = d.textlength(t, font=font(24))
            d.rounded_rectangle([x, yy, x + w + 34, yy + 44], 10, fill=(36, 44, 52))
            d.text((x + 17, yy + 8), t, font=font(24), fill=TEXT, anchor="la")
            x += w + 46
    pill_row(y)

    y = header_h
    # features
    d.rectangle([0, y, W, y + feature_h], fill=BG)
    d.text((48, y + 40), "为什么更安心 · 干净独立随时可退", font=font(28), fill=ACCENT, anchor="la")
    feats = [
        ("纯显示层汉化", "不改 SpeedTree 原程序，不改模型工程数据。"),
        ("覆盖核心界面", "菜单、工具栏、参数面板、对话框与节点参数。"),
        ("中英随时切换", "按 F3 或点击「中 / 英」，无需退出软件。"),
        ("版本自动适配", "兼容 9.x / 10.x，自动识别 Qt5 / Qt6。"),
    ]
    ty = y + 96
    for title, copy in feats:
        d.rectangle([48, ty, 96, ty + 36], fill=ACCENT)
        d.text((56, ty + 3), "✓", font=font(26), fill=(10, 14, 18), anchor="la")
        d.text((118, ty), title, font=font(34), fill=TEXT, anchor="la")
        d.text((118, ty + 46), copy, font=font(26), fill=MUTED, anchor="la")
        ty += 100

    y = header_h + feature_h
    caps = ["整体界面 · 中文菜单与工具栏", "生成 / 材质参数面板 · 中文参数", "右键菜单 · 中文操作项"]
    for idx, p in enumerate(pics):
        d.rectangle([0, y, W, y + 10], fill=(40, 46, 54))
        y += 10
        img.paste(p, (0, y))
        y += p.height
        d.text((48, y + 20), caps[idx], font=font(28), fill=ACCENT, anchor="la")
        y += cap_h

    # footer
    d.rectangle([0, y, W, y + foot_h], fill=(20, 26, 33))
    d.rectangle([0, y + foot_h - 10, W, y + foot_h], fill=ACCENT)
    d.text((48, y + 48), "纯显示层方案 · 你的 .spm 工程数据保持原样", font=font(28), fill=TEXT, anchor="la")
    d.text((48, y + 108), "不替换原始程序 · 卸载即恢复原样", font=font(24), fill=MUTED, anchor="la")
    return img


d = detail_strip()
d.save(os.path.join(OUT, "详情长图.png"), "PNG", optimize=True)

print("generated:")
for n in ["主图-1.png", "主图-2.png", "主图-3.png", "主图-4.png", "详情长图.png"]:
    p = os.path.join(OUT, n)
    print(" ", n, f"{os.path.getsize(p)/1024:.0f}KB", Image.open(p).size)
