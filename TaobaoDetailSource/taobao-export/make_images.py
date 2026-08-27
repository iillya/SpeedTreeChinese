# -*- coding: utf-8 -*-
"""Prep images for the Taobao detail page (max 750px wide, lightweight PNG)."""
from PIL import Image
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(HERE), "public", "speedtree-cn-ui.png")
OUT = os.path.join(HERE, "detail-images")
os.makedirs(OUT, exist_ok=True)

im = Image.open(SRC).convert("RGB")
W, H = im.size
print("source", W, H)


def save(img, name, width=750):
    if img.width > width:
        h = round(img.height * width / img.width)
        img = img.resize((width, h), Image.LANCZOS)
    path = os.path.join(OUT, name)
    img.save(path, "PNG", optimize=True)
    kb = os.path.getsize(path) / 1024
    print(f"{name}: {img.width}x{img.height}  {kb:.0f}KB")


# 1) 整体界面：完整截图压到 750 宽
save(im, "01-overview.png")

# 2) 左侧“属性/生成”参数面板局部
left = im.crop((0, 520, 750, H))
save(left, "02-panel-params.png")

# 3) 中间右键菜单 + 3D 视口局部
menu = im.crop((470, 540, 1350, 1300))
save(menu, "03-panel-menu.png")

print("done ->", OUT)
