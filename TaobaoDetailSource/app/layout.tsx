import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'SpeedTree 中文补丁｜淘宝详情页',
  description: 'SpeedTree Modeler 中文本地化工具商品详情页',
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return <html lang="zh-CN"><body>{children}</body></html>;
}
