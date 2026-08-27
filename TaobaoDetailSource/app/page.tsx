import Image from 'next/image';

const features = [
  ['纯显示层汉化', '不改 SpeedTree 原程序，不改模型工程数据，卸载后恢复原样。'],
  ['覆盖核心界面', '菜单、工具栏、参数面板、对话框与节点参数，常用功能一眼看懂。'],
  ['中英随时切换', '按 F3 或点击“中 / 英”，无需退出软件即可切换显示语言。'],
  ['版本自动适配', '兼容 SpeedTree Modeler 9.x / 10.x，安装器自动识别 Qt5 / Qt6。'],
];

const steps = [
  ['01', '关闭 SpeedTree', '安装前先退出正在运行的软件。'],
  ['02', '运行安装器', '自动定位版本并创建独立的 ChineseLauncher 目录。'],
  ['03', '从中文启动器进入', '使用桌面“SpeedTree 中文版”快捷方式开始工作。'],
];

export default function Home() {
  return (
    <main className="page-shell">
      <section className="hero section">
        <div className="leaf leaf-one" /><div className="leaf leaf-two" />
        <div className="eyebrow">SPEEDTREE MODELER 中文补丁</div>
        <h1>让 SpeedTree 创作<br /><em>回到创作本身</em></h1>
        <p className="hero-copy">告别反复查词。熟悉的中文界面，让建模、材质与季节调整更直观。</p>
        <div className="hero-pills"><span>9.x / 10.x</span><span>Qt5 / Qt6</span><span>Windows</span></div>
        <div className="hero-window">
          <div className="window-bar"><i /><i /><i /><b>SpeedTree Modeler 10.2.0</b></div>
          <Image src="/speedtree-cn-ui.png" alt="SpeedTree 中文界面实际效果" width={2569} height={1483} priority />
        </div>
        <p className="proof-note">实机界面截图 · 中文菜单、参数面板与右键菜单同步呈现</p>
      </section>

      <section className="section intro">
        <div className="section-kicker">不改变工作流，只减少理解成本</div>
        <h2>看得懂，调得快</h2>
        <p>节点参数不再靠猜，工具位置无需死记。汉化只作用于界面显示，你的 SpeedTree 工程仍是原来的工程。</p>
        <div className="stat-row"><div><strong>F3</strong><span>一键中英切换</span></div><div><strong>2</strong><span>代 Qt 框架适配</span></div><div><strong>0</strong><span>原程序文件改写</span></div></div>
      </section>

      <section className="section features">
        <div className="section-kicker light">为什么更安心</div><h2>干净、独立、随时可退</h2>
        <div className="feature-grid">{features.map(([title, copy], index) => <article key={title}><span>0{index + 1}</span><h3>{title}</h3><p>{copy}</p></article>)}</div>
      </section>

      <section className="section coverage">
        <div className="section-kicker">常用区域重点覆盖</div><h2>从主菜单到节点参数</h2>
        <div className="coverage-list">{['顶部菜单与工具栏', '生成器与节点参数', '材质、季节与网格面板', '对话框与右键菜单'].map(item => <div key={item}><i>✓</i><span>{item}</span></div>)}</div>
        <div className="crop-frame"><Image src="/speedtree-cn-ui.png" alt="SpeedTree 中文参数面板细节" width={2569} height={1483} /></div>
      </section>

      <section className="section steps">
        <div className="section-kicker light">安装很简单</div><h2>三步进入中文界面</h2>
        {steps.map(([num, title, copy]) => <article key={num}><b>{num}</b><div><h3>{title}</h3><p>{copy}</p></div></article>)}
      </section>

      <section className="section safety">
        <div className="section-kicker">纯显示层方案</div><h2>不碰模型数据<br />不替换原始程序</h2>
        <p>汉化文件统一放入原软件目录下的 <code>ChineseLauncher</code>，与原程序职责分离。卸载时可恢复文件关联，原软件文件不受影响。</p>
        <div className="shield"><span>✓</span><b>你的 .spm 工程数据<br />保持原样</b></div>
      </section>

      <section className="section compatibility">
        <div className="section-kicker light">兼容范围</div><h2>一份安装器，自动识别</h2>
        <div className="version-card"><b>SpeedTree 10.x</b><span>Qt 6.6 系列</span><i>支持</i></div>
        <div className="version-card"><b>SpeedTree 9.x</b><span>Qt 5.12 系列</span><i>支持</i></div>
        <p className="minor">适用于 Windows。具体小版本请以商品说明与实际检测结果为准。</p>
      </section>

      <section className="section faq">
        <div className="section-kicker">购买前常见问题</div><h2>你可能还想知道</h2>
        <dl>
          <div><dt>会修改我的工程文件吗？</dt><dd>不会。补丁只负责界面文字显示，不改写 .spm 模型数据。</dd></div>
          <div><dt>可以临时切回英文吗？</dt><dd>可以。默认使用 F3，也可从“中 / 英”菜单切换。</dd></div>
          <div><dt>卸载后影响原软件吗？</dt><dd>不影响。卸载会移除汉化启动文件并恢复相关关联。</dd></div>
          <div><dt>商品包含 SpeedTree 软件吗？</dt><dd>不包含。本商品仅为中文本地化工具，请自行准备合法授权的软件。</dd></div>
        </dl>
      </section>

      <section className="section notice">
        <h2>购买须知</h2><ul>
          <li>本商品为 SpeedTree Modeler 中文本地化工具，不包含软件本体及授权。</li>
          <li>安装前请关闭 SpeedTree，并从“SpeedTree 中文版”快捷方式启动。</li>
          <li>不同版本界面与词条可能略有差异，请先核对自己的软件版本。</li>
          <li>数字商品具有可复制性，交付与售后规则请以店铺页面约定为准。</li>
        </ul>
      </section>
      <footer><b>SpeedTree 中文补丁</b><span>让工具更顺手，让创作更专注</span></footer>
    </main>
  );
}
