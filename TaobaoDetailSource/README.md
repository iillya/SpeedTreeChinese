# SpeedTree 淘宝详情页源码

这是一个按淘宝详情页常用宽度制作的 750px 单页，已使用真实汉化截图。

## 预览

```powershell
npm install
npm run dev
```

浏览器打开 `http://localhost:3000/`。

## 修改位置

- 页面文字与版块：`app/page.tsx`
- 配色、字号与排版：`app/globals.css`
- 汉化实机截图：`public/speedtree-cn-ui.png`

## 构建

```powershell
npm run build
npm run start
```

页面固定视觉宽度为 750px，小屏幕会自动缩放。商品主图价格、店铺承诺、联系方式等未擅自填写，请在发布时按实际情况补充。
