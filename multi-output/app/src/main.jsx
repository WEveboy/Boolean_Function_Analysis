import React, { useMemo, useState } from 'react'
import { createRoot } from 'react-dom/client'
import './styles.css'

const groups = [
  { title: '基础与代数', subtitle: '输出分布、次数和代数抗性', items: [
    ['balance', '平衡性', '各输出出现次数是否相同'],
    ['max_degree', '最大代数次数', '坐标函数中最高的 ANF 次数'],
    ['min_degree', '最小代数次数', '全部非零分量中的最低次数'],
    ['ai', '代数免疫阶 AI', '全部非零分量的最小 AI；n≤8、m≤6']
  ] },
  { title: '频谱与非线性', subtitle: '按需计算输出分量的 Walsh 谱', items: [
    ['nonlinearity', '向量非线性度', '所有非零输出分量中的最小值'],
    ['resiliency', '弹性阶', '低重量 Walsh 系数判据'],
    ['bent', '多输出 Bent', '全部非零分量满足 Bent 条件'],
    ['almost_optimal', '几乎最优', '比较书中相应维数门槛']
  ] },
  { title: 'S 盒差分分析', subtitle: '差分分布表 DDT 与相关指标', items: [
    ['diff_uniformity', '差分均匀度', 'DDT 非零输入差分行的最大值'],
    ['diff_probability', '最大差分概率', '差分均匀度除以 2ⁿ'],
    ['diff_deviation', '差分最大偏差', 'DDT 单元偏离理想均值的最大值'],
    ['diff_stddev', '差分标准差', '全体非零差分单元的均方根'],
    ['pn', 'PN 判定', '完全非线性'],
    ['apn', 'APN 判定', '方阵且差分均匀度为 2']
  ] },
  { title: 'S 盒线性分析', subtitle: '线性近似表 LAT 与相关指标', items: [
    ['linearity', '最大线性相关幅度', '非零输入和输出掩码的最大 |LAT|'],
    ['linear_bias', '最大绝对线性偏差', '相关幅度换算为概率偏差'],
    ['linear_probability', '最佳线性近似概率', '等式或互补式的最大命中概率'],
    ['ab', 'AB 判定', '结合 APN 与非线性度']
  ] }
]
const ids = groups.flatMap(group => group.items.map(item => item[0]))
const defaults = new Set(['balance', 'nonlinearity', 'diff_uniformity', 'linearity'])
function baseName(path) { return path?.split(/[\\/]/).pop() || '' }

function MetricTile({ item, active, onClick }) {
  const [id, title, detail] = item
  return <button className={`metric-tile ${active ? 'selected' : ''}`} type="button" onClick={() => onClick(id)} aria-pressed={active}>
    <span className="tick">{active ? '✓' : ''}</span><span><strong>{title}</strong><small>{detail}</small></span>
  </button>
}

function App() {
  const [input, setInput] = useState('')
  const [outputDir, setOutputDir] = useState('')
  const [n, setN] = useState(4)
  const [m, setM] = useState(4)
  const [radix, setRadix] = useState('hex')
  const [transpose, setTranspose] = useState(false)
  const [metrics, setMetrics] = useState(defaults)
  const [ddt, setDdt] = useState(false)
  const [lat, setLat] = useState(false)
  const [result, setResult] = useState(null)
  const [error, setError] = useState('')
  const [busy, setBusy] = useState(false)
  const count = useMemo(() => metrics.size, [metrics])
  const toggle = id => setMetrics(current => { const next = new Set(current); next.has(id) ? next.delete(id) : next.add(id); return next })
  const chooseInput = async () => { const p = await window.vbfDesktop.pickInput(); if (p) { setInput(p); setResult(null); setError('') } }
  const dropInput = event => {
    event.preventDefault()
    const file = event.dataTransfer?.files?.[0]
    if (file) { const p = window.vbfDesktop.pathForFile(file); if (p) { setInput(p); setResult(null); setError('') } }
  }
  const run = async () => {
    setBusy(true); setError(''); setResult(null)
    try {
      const data = await window.vbfDesktop.analyze({ input, outputDir, n: Number(n), m: Number(m), radix, transpose, metrics: [...metrics], ddt, lat })
      if (!data.ok) setError(data.error || '计算失败。')
      else setResult(data)
    } catch (e) { setError(e.message || String(e)) }
    finally { setBusy(false) }
  }
  return <div className="shell">
    <aside className="sidebar">
      <div className="brand"><div className="brand-icon">F</div><div><strong>向量布尔函数实验室</strong><small>VECTORIAL FUNCTION LAB</small></div></div>
      <div className="sidebar-label">分析工作台</div>
      <div className="nav active">▦ <span>安全性指标分析</span></div>
      <div className="nav">◇ <span>S 盒 DDT / LAT</span></div>
      <div className="sidebar-spacer" />
      <div className="sidebar-card"><strong>本地计算</strong><p>输入在设备上交给 C++ 引擎处理。勾选的指标才运行，完整 DDT 和 LAT 单独导出。</p></div>
      <div className="sidebar-foot">多输出版本 <span>v1.0.1</span></div>
    </aside>
    <main className="main">
      <header className="topbar"><span>工作台 <b>/</b> 多输出布尔函数</span><span className="local-dot">● 本地模式</span></header>
      <div className="content">
        <div className="heading"><div><div className="eyebrow">CRYPTOGRAPHIC FUNCTION ANALYSIS</div><h1>多输出布尔函数安全性分析</h1><p>从向量真值表导入 S 盒，分别分析差分分布与线性近似。</p></div><div className="heading-symbol">F<span>₂ⁿ → F₂ᵐ</span></div></div>
        <div className="step"><span>01</span><div><h2>导入函数</h2><p>{transpose ? '每行一个坐标函数，行内按输入 0 到 2ⁿ−1 排列' : '输出词按输入 0 到 2ⁿ−1 的顺序排列'}</p></div></div>
        <section className="surface input-surface">
          <div className={`dropzone ${input ? 'filled' : ''}`} onDragOver={e => e.preventDefault()} onDrop={dropInput}>
            <div className="drop-icon">⇧</div><div className="drop-copy"><strong>{input ? baseName(input) : '拖入 TXT 文件或点击浏览'}</strong><small>{input || (transpose ? '每行一个坐标函数，按 f1 至 fm 排列' : '每个输出词可按空格、逗号或换行分隔；也支持连续定宽串')}</small></div><button type="button" onClick={chooseInput}>{input ? '更换文件' : '浏览文件'}</button>
          </div>
          <div className="settings">
            <label>输入位数 n<input type="number" min="1" max="12" value={n} onChange={e => { setN(e.target.value); if (Number(m) > Number(e.target.value)) setM(e.target.value) }} /></label>
            <label>输出位数 m<input type="number" min="1" max="8" value={m} onChange={e => setM(e.target.value)} /></label>
            <label>真值表进制<select value={radix} onChange={e => setRadix(e.target.value)}><option value="hex">十六进制</option><option value="bin">二进制</option></select></label>
            <label>输出位置<button type="button" className="folder" onClick={async () => { const p = await window.vbfDesktop.pickOutput(); if (p) setOutputDir(p) }}>{outputDir ? baseName(outputDir) : '默认输入文件旁'}</button></label>
          </div>
          <label className="transpose-option"><input type="checkbox" checked={transpose} onChange={e => { setTranspose(e.target.checked); setResult(null); setError('') }} /><span><strong>转置真值表</strong><small>文件按 f1 到 fm 逐行存放；每行包含 2ⁿ 个输入位置的 0/1 值，或将该行位串按四位一组写成十六进制。</small></span></label>
          <p className="hint">{transpose ? <>转置示例（4×4 PRESENT，十六进制）：四行分别为 <code>9B70</code>、<code>E16C</code>、<code>32E5</code>、<code>59A6</code>。保留行首 0；转置后仍导出标准逐词真值表。</> : <>例如 4×4 PRESENT S 盒：<code>C 5 6 B 9 0 A D 3 E F 8 4 7 1 2</code>。十六进制输出词宽度为 ⌈m/4⌉ 位；二进制为 m 位。</>}</p>
        </section>
        <div className="step step-two"><span>02</span><div><h2>选择指标</h2><p>勾选后按需运行；标准化真值表与坐标 ANF 始终导出</p></div><div className="selection">已选 {count} 项 <button onClick={() => setMetrics(new Set(ids))}>全选</button><button onClick={() => setMetrics(new Set())}>清空</button></div></div>
        <div className="groups">{groups.map(group => <section className="surface metric-group" key={group.title}><div className="group-head"><div><h3>{group.title}</h3><p>{group.subtitle}</p></div><span>{group.items.length} 项</span></div><div className="tiles">{group.items.map(item => <MetricTile key={item[0]} item={item} active={metrics.has(item[0])} onClick={toggle} />)}</div></section>)}</div>
        <section className="surface tables"><div><strong>完整表格导出</strong><p>单独请求；仅分析摘要时不会创建完整表。</p></div><label><input type="checkbox" checked={ddt} onChange={e => setDdt(e.target.checked)} /> 差分分布表 DDT.csv</label><label><input type="checkbox" checked={lat} onChange={e => setLat(e.target.checked)} /> 线性近似表 LAT.csv</label></section>
        <div className="action"><div><strong>准备开始</strong><p>计算结果在界面展示，文件保存到所选目录。</p></div><button disabled={!input || busy} onClick={run}>{busy ? '正在计算…' : '开始分析 →'}</button></div>
        {error && <div className="error">{error}</div>}
        {result && <section className="results"><div className="step"><span>✓</span><div><h2>分析结果</h2><p>{result.input?.name} · {result.input?.n} 输入位 / {result.input?.m} 输出位 · {result.input?.transposed ? '已转置输入 · ' : ''}{result.input?.permutation ? '置换 S 盒' : '向量函数'}</p></div></div><div className="success">计算完成 · 指标由本地 C++ 引擎生成</div><div className="cards">{result.results?.length ? result.results.map(item => <article className="result-card" key={item.id}><small>{item.title}</small><strong>{item.value}</strong><p>{item.detail}</p></article>) : <div className="empty">未选择指标，已导出标准化真值表和 ANF。</div>}</div><div className="surface files"><strong>生成的文件</strong><div>{result.files?.map(file => <button key={file} onClick={() => window.vbfDesktop.openFile(file)}>{baseName(file)} ↗</button>)}</div></div></section>}
        <footer>多输出布尔函数安全性分析 · 本机离线运行</footer>
      </div>
    </main>
  </div>
}
createRoot(document.getElementById('root')).render(<App />)
