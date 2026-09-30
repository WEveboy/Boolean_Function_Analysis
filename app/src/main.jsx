import React, { useMemo, useRef, useState } from 'react'
import { createRoot } from 'react-dom/client'
import MultiOutput from './MultiOutput.jsx'
import './styles.css'

const metricGroups = [
  { title: '基础性质', subtitle: '快速了解函数结构', items: [
    ['balance', '平衡性', '输出 0 / 1 是否均匀'],
    ['degree', '代数次数', 'ANF 的最高次数'],
    ['nonlinearity', '非线性度', '到仿射函数的最短距离'],
    ['bent', 'Bent 判定', '检查最大非线性度'],
    ['plateaued', 'Plateaued 分类', '判断谱值形态']
  ] },
  { title: '频谱与相关', subtitle: 'Walsh 谱分析', items: [
    ['walsh_dist', '频谱次数分布', '统计每个谱值出现次数'],
    ['correlation_immunity', '相关免疫阶', '低重量谱点的零值条件'],
    ['resiliency', '弹性阶', '平衡与相关免疫联合判定']
  ] },
  { title: '扩散与雪崩', subtitle: '输入变化的影响', items: [
    ['sac', '严格雪崩准则', '单比特翻转时输出是否平衡'],
    ['pc', '扩散准则 PC(k)', '检查指定阶数的差分'],
    ['gac', '全局雪崩特征', 'Δ 与 σ 两项指标'],
    ['autocorr_dist', '自相关值分布', '次数统计与绝对值极值']
  ] },
  { title: '代数安全', subtitle: '精确算法有规模限制', items: [
    ['ai', '代数免疫阶 AI', 'GF(2) 线性系统求解'],
    ['fai', '快速代数免疫阶 FAI', '小规模穷举与剪枝']
  ] },
  { title: '函数变换', subtitle: '可选的高级功能', items: [
    ['ea', 'EA 变换', '给定可逆矩阵与仿射参数，计算变换后已选指标']
  ] }
]

const initialSelected = new Set(['balance', 'degree', 'nonlinearity'])

function Icon({ name, size = 20 }) {
  const common = { width: size, height: size, viewBox: '0 0 24 24', fill: 'none', stroke: 'currentColor', strokeWidth: 1.8, strokeLinecap: 'round', strokeLinejoin: 'round' }
  const shapes = {
    upload: <><path d="M12 16V4m0 0-4 4m4-4 4 4"/><path d="M4 16v3a1 1 0 0 0 1 1h14a1 1 0 0 0 1-1v-3"/></>,
    file: <><path d="M6 3h8l4 4v14H6a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2Z"/><path d="M14 3v5h5M8 13h8M8 17h6"/></>,
    folder: <><path d="M3 7a2 2 0 0 1 2-2h5l2 2h7a2 2 0 0 1 2 2v10H3V7Z"/></>,
    play: <path d="m8 5 11 7-11 7V5Z"/>,
    check: <path d="m5 12 4 4L19 6"/>,
    arrow: <><path d="M4 12h15m0 0-5-5m5 5-5 5"/></>,
    spark: <><path d="m12 2 1.8 6.2L20 10l-6.2 1.8L12 18l-1.8-6.2L4 10l6.2-1.8L12 2Z"/><path d="m19 17 .7 2.3L22 20l-2.3.7L19 23l-.7-2.3L16 20l2.3-.7L19 17Z"/></>,
    chart: <><path d="M4 19V5M4 19h16M8 15l3-5 3 2 4-7"/></>,
    close: <path d="M5 5 19 19M19 5 5 19"/>,
    info: <><circle cx="12" cy="12" r="9"/><path d="M12 11v5m0-8h.01"/></>
  }
  return <svg {...common} aria-hidden="true">{shapes[name]}</svg>
}

function shortName(filePath) {
  return filePath?.split(/[\\/]/).pop() || ''
}

function MetricCard({ item, selected, onToggle }) {
  const [id, title, desc] = item
  return <button type="button" className={`metric-tile ${selected ? 'selected' : ''}`} onClick={() => onToggle(id)} aria-pressed={selected}>
    <span className="metric-checkbox">{selected && <Icon name="check" size={14} />}</span>
    <span className="metric-copy"><strong>{title}</strong><small>{desc}</small></span>
  </button>
}

function FileDrop({ index, title, filePath, onFiles, onBrowse, subtitle }) {
  const [over, setOver] = useState(false)
  const handleDrop = (event) => {
    event.preventDefault()
    setOver(false)
    const files = Array.from(event.dataTransfer?.files || [])
    if (files.length) onFiles(files)
  }
  return <div className={`file-drop ${over ? 'over' : ''} ${filePath ? 'filled' : ''}`}
    onDragOver={e => { e.preventDefault(); setOver(true) }}
    onDragLeave={() => setOver(false)} onDrop={handleDrop}>
    <div className="drop-icon"><Icon name={filePath ? 'file' : 'upload'} size={23} /></div>
    <div className="drop-text">
      <span className="eyebrow">{index}</span>
      <strong>{filePath ? shortName(filePath) : title}</strong>
      <small>{filePath ? filePath : subtitle}</small>
    </div>
    <button type="button" className="browse-button" onClick={onBrowse}>{filePath ? '更换' : '浏览'}</button>
  </div>
}

function Distribution({ counts, total }) {
  const [page, setPage] = useState(0)
  if (!counts?.length) return null
  const pageSize = 100
  const pages = Math.ceil(counts.length / pageSize)
  const currentPage = Math.min(page, pages - 1)
  const visible = counts.slice(currentPage * pageSize, (currentPage + 1) * pageSize)
  const max = counts.reduce((a, item) => Math.max(a, item.count), 0) || 1
  return <div className="distribution">
    <div className="distribution-title"><span>数值</span><span>出现次数</span></div>
    <div className="distribution-scroll">{visible.map(item => <div className="distribution-row" key={item.value}>
      <span className="distribution-value">{item.value}</span>
      <div className="distribution-track"><div style={{ width: `${Math.max(3, item.count / max * 100)}%` }} /></div>
      <span className="distribution-count">{item.count}</span>
    </div>)}</div>
    {pages > 1 && <div className="distribution-pager"><button disabled={currentPage === 0} onClick={() => setPage(currentPage - 1)}>上一页</button><span>第 {currentPage + 1} / {pages} 页 · 共 {total || counts.length} 种</span><button disabled={currentPage + 1 >= pages} onClick={() => setPage(currentPage + 1)}>下一页</button></div>}
  </div>
}

function ResultCard({ result }) {
  return <section className="result-card">
    <div className="result-head"><div className="result-icon"><Icon name="chart" size={18} /></div><span>{result.title}</span></div>
    <div className="result-value">{result.value}</div>
    {result.detail && <p className="result-detail">{result.detail}</p>}
    <Distribution counts={result.counts} total={result.countTotal} />
    {result.stats && <div className="stats-row">
      <div><small>绝对值最小</small><strong>{result.stats.minAbs}</strong></div>
      <div><small>绝对值最大</small><strong>{result.stats.maxAbs}</strong></div>
      {result.stats.nonzeroMinAbs !== undefined && <div><small>非零差分最小</small><strong>{result.stats.nonzeroMinAbs}</strong></div>}
      {result.stats.nonzeroMaxAbs !== undefined && <div><small>非零差分最大</small><strong>{result.stats.nonzeroMaxAbs}</strong></div>}
    </div>}
  </section>
}

function SingleOutput() {
  const [mode, setMode] = useState('single')
  const [files, setFiles] = useState(['', ''])
  const [kind, setKind] = useState('auto')
  const [radix, setRadix] = useState('auto')
  const [n, setN] = useState('')
  const [pcK, setPcK] = useState('1')
  const [eaRows, setEaRows] = useState('')
  const [eaAlpha, setEaAlpha] = useState('')
  const [eaBeta, setEaBeta] = useState('')
  const [eaEpsilon, setEaEpsilon] = useState('0')
  const [outputDir, setOutputDir] = useState('')
  const [selected, setSelected] = useState(initialSelected)
  const [running, setRunning] = useState(false)
  const [result, setResult] = useState(null)
  const [error, setError] = useState('')
  const resultRef = useRef(null)

  const selectedCount = mode === 'cross' ? (selected.has('crosscorr_dist') ? 1 : 0) : selected.size
  const canRun = Boolean(files[0] && (mode === 'single' || files[1])) && !running
  const allIds = useMemo(() => metricGroups.flatMap(group => group.items.map(item => item[0])), [])

  function changeMode(next) {
    setMode(next)
    setFiles(['', ''])
    setKind('auto')
    setN('')
    setResult(null)
    setError('')
    setSelected(next === 'cross' ? new Set(['crosscorr_dist']) : new Set(initialSelected))
  }
  function toggle(id) {
    setSelected(current => { const next = new Set(current); next.has(id) ? next.delete(id) : next.add(id); return next })
  }
  function acceptPaths(paths, target) {
    const clean = paths.filter(Boolean).filter(p => p.toLowerCase().endsWith('.txt'))
    if (clean.length !== paths.length) { setError('只能拖入 TXT 文本文件。'); return }
    setFiles(current => {
      const next = [...current]
      if (mode === 'cross' && clean.length >= 2) { next[0] = clean[0]; next[1] = clean[1] }
      else next[target] = clean[0] || ''
      return next
    })
    setError('')
    setResult(null)
  }
  function acceptDropFiles(fileObjects, target) {
    const paths = fileObjects.map(file => window.bfDesktop.pathForFile(file))
    acceptPaths(paths, target)
  }
  async function browse(target) {
    const paths = await window.bfDesktop.pickFiles(mode === 'cross' && !files[0] && target === 0 ? 2 : 1)
    if (paths?.length) acceptPaths(paths, target)
  }
  async function run() {
    if (!canRun) return
    setRunning(true)
    setError('')
    setResult(null)
    try {
      const response = await window.bfDesktop.run({
        input: files[0], input2: mode === 'cross' ? files[1] : '',
        kind: mode === 'cross' ? 'truth' : kind,
        radix, n: n ? Number(n) : undefined, pcK: Number(pcK),
        outputDir, eaRows, eaAlpha, eaBeta, eaEpsilon,
        metrics: mode === 'cross' ? (selected.has('crosscorr_dist') ? ['crosscorr_dist'] : []) : [...selected]
      })
      if (!response?.ok) throw new Error(response?.error || '计算失败')
      setResult(response)
      setTimeout(() => resultRef.current?.scrollIntoView({ behavior: 'smooth', block: 'start' }), 80)
    } catch (e) { setError(e.message || '计算失败') }
    finally { setRunning(false) }
  }

  return <main className="main-content">
      <div className="topbar"><div className="breadcrumb">工作空间 <span>/</span> 单输出布尔函数</div><div className="topbar-status"><span className="online-dot"/> 本地运行 · 无需上传</div></div>
      <div className="content-wrap">
        <header className="page-header"><div><div className="page-kicker">BOOLEAN FUNCTION ANALYSIS</div><h1>单输出布尔函数安全性分析</h1><p>导入真值表或代数正规型，按需选择指标，获得可追溯的计算结果。</p></div><div className="header-decoration">Σ<span>f(x)</span></div></header>

        <div className="step-line"><span className="step-number">01</span><div><h2>导入函数</h2><p>拖入 TXT 文件，或从电脑中选择</p></div></div>
        <section className="surface input-surface">
          <div className="mode-switch"><button className={mode === 'single' ? 'active' : ''} onClick={() => changeMode('single')}>单函数分析</button><button className={mode === 'cross' ? 'active' : ''} onClick={() => changeMode('cross')}>双函数互相关</button></div>
          <div className={`drop-grid ${mode === 'cross' ? 'two' : ''}`}>
            <FileDrop index={mode === 'cross' ? '函数 f' : 'TXT 文件'} title="拖入真值表或 ANF 文件" subtitle="支持二进制、十六进制真值表及代数正规型" filePath={files[0]} onFiles={v => acceptDropFiles(v, 0)} onBrowse={() => browse(0)} />
            {mode === 'cross' && <FileDrop index="函数 g" title="拖入第二份真值表" subtitle="进制和长度必须与函数 f 一致" filePath={files[1]} onFiles={v => acceptDropFiles(v, 1)} onBrowse={() => browse(1)} />}
          </div>
          <div className="options-grid">
            {mode === 'single' && <label><span>输入类型</span><select value={kind} onChange={e => setKind(e.target.value)}><option value="auto">自动识别</option><option value="truth">真值表</option><option value="anf">代数正规型</option></select></label>}
            <label><span>真值表进制</span><select value={radix} onChange={e => setRadix(e.target.value)}><option value="auto">自动识别</option><option value="bin">二进制</option><option value="hex">十六进制</option></select></label>
            {mode === 'single' && <label><span>变量数 n <em>可选</em></span><input type="number" min="1" max="20" placeholder="自动推断" value={n} onChange={e => setN(e.target.value)} /></label>}
            <label><span>保存位置 <em>可选</em></span><button className="output-picker" onClick={async () => { const p = await window.bfDesktop.pickOutput(); if (p) setOutputDir(p) }}><Icon name="folder" size={17}/><span>{outputDir ? shortName(outputDir) : '默认源文件旁'}</span></button></label>
          </div>
          <div className="helper-line"><Icon name="info" size={16}/><span>仅由 0 / 1 构成的内容默认按二进制读取；若为十六进制，请加 0x 前缀或在上方指定。</span></div>
        </section>

        <div className="step-line metrics-step"><span className="step-number">02</span><div><h2>选择需要的指标</h2><p>只计算已勾选的项目；真值表与 ANF 文本始终生成</p></div><div className="selection-actions"><span>已选 {selectedCount} 项</span>{mode === 'single' && <><button onClick={() => setSelected(new Set(allIds))}>全选</button><button onClick={() => setSelected(new Set())}>清空</button></>}</div></div>
        {mode === 'cross' ? <section className="surface cross-surface"><MetricCard item={['crosscorr_dist', '互相关值分布', '统计所有互相关值的次数、绝对值最大与最小']} selected={selected.has('crosscorr_dist')} onToggle={toggle} /><p>双文件模式只处理两份进制和长度一致的真值表；两个导出文件对应函数 f。</p></section> : <div className="metric-groups">{metricGroups.map(group => <section className="surface metric-group" key={group.title}><div className="group-head"><div><h3>{group.title}</h3><p>{group.subtitle}</p></div><span>{group.items.length} 项</span></div><div className="metric-grid">{group.items.map(item => <MetricCard key={item[0]} item={item} selected={selected.has(item[0])} onToggle={toggle} />)}</div>{group.title === '扩散与雪崩' && selected.has('pc') && <label className="pc-input">扩散阶数 k <input type="number" min="1" max="20" value={pcK} onChange={e => setPcK(e.target.value)} /></label>}{group.title === '函数变换' && selected.has('ea') && <div className="ea-options"><p>留空使用单位矩阵与零向量；矩阵行用英文逗号分隔，例如 n=3 时 100,010,001。</p><label>矩阵 A<input value={eaRows} onChange={e => setEaRows(e.target.value)} placeholder="默认单位矩阵" /></label><label>输入平移 α<input value={eaAlpha} onChange={e => setEaAlpha(e.target.value)} placeholder="默认全 0" /></label><label>输出线性项 β<input value={eaBeta} onChange={e => setEaBeta(e.target.value)} placeholder="默认全 0" /></label><label>常数项 ε<select value={eaEpsilon} onChange={e => setEaEpsilon(e.target.value)}><option value="0">0</option><option value="1">1</option></select></label></div>}</section>)}</div>}

        <div className="action-bar"><div><strong>准备就绪</strong><p>计算结果与标准化 TXT 将保存在本地。</p></div><button className="run-button" disabled={!canRun} onClick={run}>{running ? <span className="spinner" /> : <Icon name="play" size={18} />}{running ? '正在计算…' : '开始分析'}<Icon name="arrow" size={18}/></button></div>
        {error && <div className="error-banner"><Icon name="info" size={18}/><span>{error}</span><button onClick={() => setError('')}><Icon name="close" size={16}/></button></div>}

        <div ref={resultRef} />
        {result && <section className="result-section"><div className="step-line"><span className="step-number done"><Icon name="check" size={17}/></span><div><h2>分析结果</h2><p>{result.inputs?.map(i => `${i.name} · ${i.n} 元`).join('  /  ')}</p></div></div>
          <div className="result-banner"><div><Icon name="check" size={20}/></div><span>计算完成</span><small>所有结果均由本地 C++ 引擎生成</small></div>
          <div className="results-grid">{result.results?.length ? result.results.map((item, index) => <ResultCard result={item} key={item.id + index} />) : <div className="empty-results">未选择指标；已完成真值表与 ANF 转换。</div>}</div>
          {result.transformedResults?.length > 0 && <><div className="step-line transformed-step"><span className="step-number">EA</span><div><h2>变换后的指标</h2><p>仅重新计算已勾选的指标，便于与原函数比较。</p></div></div><div className="results-grid">{result.transformedResults.map((item, index) => <ResultCard result={item} key={item.id + index} />)}</div></>}
          <div className="exports surface"><div className="exports-heading"><Icon name="folder" size={19}/><div><strong>已生成的结果文件</strong><p>点击文件可在系统中打开</p></div></div><div className="export-list">{result.files?.map((file, index) => <button key={index} onClick={() => window.bfDesktop.openOutput(file)}><Icon name="file" size={17}/><span>{shortName(file)}</span><Icon name="arrow" size={16}/></button>)}</div></div>
        </section>}
        <footer className="page-footer">布尔函数安全性分析 · 所有文件仅在此设备上处理</footer>
      </div>
    </main>
}

function App() {
  const [page, setPage] = useState('single')
  return <div className="shell">
    <aside className="sidebar">
      <div className="brand"><div className="brand-logo">ƒ</div><div><strong>布尔函数实验室</strong><small>Security Analysis Studio</small></div></div>
      <div className="nav-section-label">分析类型</div>
      <nav className="sidebar-nav" aria-label="分析类型">
        <button type="button" className={`nav-item ${page === 'single' ? 'active' : ''}`} aria-current={page === 'single' ? 'page' : undefined} onClick={() => setPage('single')}><Icon name="chart" size={18}/><span>单输出布尔函数</span>{page === 'single' && <span className="nav-active-dot" />}</button>
        <button type="button" className={`nav-item ${page === 'multi' ? 'active' : ''}`} aria-current={page === 'multi' ? 'page' : undefined} onClick={() => setPage('multi')}><Icon name="spark" size={18}/><span>多输出布尔函数</span>{page === 'multi' && <span className="nav-active-dot" />}</button>
      </nav>
      <div className="sidebar-space" />
      <div className="sidebar-note"><div className="sidebar-note-icon"><Icon name="spark" size={18}/></div><strong>本地计算</strong><p>输入、转换、计算与导出全部在本机完成。</p></div>
      <div className="sidebar-footer"><span className="online-dot" /> 本地 C++ 计算引擎</div>
    </aside>
    {page === 'single' ? <SingleOutput /> : <MultiOutput />}
  </div>
}

createRoot(document.getElementById('root')).render(<App />)
