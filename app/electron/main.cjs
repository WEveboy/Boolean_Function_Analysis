const { app, BrowserWindow, dialog, ipcMain, shell } = require('electron')
const path = require('node:path')
const fs = require('node:fs')
const { execFile } = require('node:child_process')

let windowRef
const allowedMetrics = new Set([
  'balance', 'degree', 'nonlinearity', 'walsh_dist', 'correlation_immunity',
  'resiliency', 'bent', 'plateaued', 'ai', 'fai', 'sac', 'pc', 'gac',
  'autocorr_dist', 'crosscorr_dist', 'ea'
])
const allowedVectorMetrics = new Set([
  'balance', 'max_degree', 'min_degree', 'ai', 'nonlinearity', 'resiliency',
  'bent', 'almost_optimal', 'diff_uniformity', 'diff_probability',
  'diff_deviation', 'diff_stddev', 'pn', 'apn', 'linearity',
  'linear_bias', 'linear_probability', 'ab'
])

function corePath() {
  return app.isPackaged
    ? path.join(process.resourcesPath, 'bin', 'bf-core.exe')
    : path.resolve(__dirname, '../../cpp/build/bf-core.exe')
}

function vectorCorePath() {
  return app.isPackaged
    ? path.join(process.resourcesPath, 'bin', 'vbf-core.exe')
    : path.resolve(__dirname, '../../multi-output/cpp/build/vbf-core.exe')
}

function createWindow() {
  windowRef = new BrowserWindow({
    width: 1480,
    height: 940,
    minWidth: 1080,
    minHeight: 700,
    backgroundColor: '#f3f5f9',
    title: '布尔函数实验室',
    autoHideMenuBar: true,
    webPreferences: {
      preload: path.join(__dirname, 'preload.cjs'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true
    }
  })
  windowRef.webContents.setWindowOpenHandler(() => ({ action: 'deny' }))
  if (!app.isPackaged && (process.env.BF_DEV_URL || !fs.existsSync(path.join(__dirname, '../dist/index.html')))) {
    windowRef.loadURL(process.env.BF_DEV_URL || 'http://127.0.0.1:5173')
  } else {
    windowRef.loadFile(path.join(__dirname, '../dist/index.html'))
  }
  if (process.env.BF_SMOKE_PATH) {
    windowRef.webContents.once('did-finish-load', () => setTimeout(async () => {
      try {
        const state = await windowRef.webContents.executeJavaScript(`(async () => {
          const initial = { title: document.title, heading: document.querySelector('h1')?.textContent,
            metricCount: document.querySelectorAll('.metric-tile').length,
            desktopApi: typeof window.bfDesktop?.run === 'function' }
          const first = document.querySelector('.metric-tile')
          const before = first?.getAttribute('aria-pressed')
          first?.click()
          await new Promise(resolve => setTimeout(resolve, 50))
          initial.checkboxToggled = before !== first?.getAttribute('aria-pressed')
          ${process.env.BF_SMOKE_INPUT ? `initial.backend = await window.bfDesktop.run({input:${JSON.stringify(process.env.BF_SMOKE_INPUT)},outputDir:${JSON.stringify(process.env.BF_SMOKE_OUT || '')},metrics:['balance','degree']})` : ''}
          document.querySelector('.sidebar-nav .nav-item:nth-child(2)')?.click()
          await new Promise(resolve => setTimeout(resolve, 80))
          initial.vectorHeading = document.querySelector('h1')?.textContent
          initial.vectorMetricCount = document.querySelectorAll('.metric-tile').length
          initial.vectorApi = typeof window.vbfDesktop?.analyze === 'function'
          initial.vectorNavActive = document.querySelector('.sidebar-nav .nav-item:nth-child(2)')?.getAttribute('aria-current') === 'page'
          initial.vectorContentWidth = Math.round(document.querySelector('.multi-page .content')?.getBoundingClientRect().width || 0)
          ${process.env.BF_SMOKE_VECTOR_INPUT ? `initial.vectorBackend = await window.vbfDesktop.analyze({input:${JSON.stringify(process.env.BF_SMOKE_VECTOR_INPUT)},outputDir:${JSON.stringify(process.env.BF_SMOKE_VECTOR_OUT || '')},n:4,m:4,radix:'hex',metrics:['balance','diff_uniformity','linearity']})` : ''}
          return initial
        })()`)
        fs.writeFileSync(process.env.BF_SMOKE_PATH + '.json', JSON.stringify(state, null, 2))
        const image = await windowRef.capturePage()
        fs.writeFileSync(process.env.BF_SMOKE_PATH, image.toPNG())
      } catch (error) {
        fs.writeFileSync(process.env.BF_SMOKE_PATH + '.error.txt', String(error.stack || error))
      } finally { app.quit() }
    }, 1000))
  }
}

app.whenReady().then(() => {
  ipcMain.handle('vector:input:pick', async () => {
    const result = await dialog.showOpenDialog(windowRef, {
      title: '选择 S 盒或向量函数 TXT',
      filters: [{ name: '文本文件', extensions: ['txt'] }],
      properties: ['openFile']
    })
    return result.canceled ? '' : result.filePaths[0]
  })
  ipcMain.handle('files:pick', async (_event, count) => {
    const result = await dialog.showOpenDialog(windowRef, {
      title: count === 2 ? '选择两份真值表 TXT' : '选择 TXT 文件',
      buttonLabel: '选择文件',
      filters: [{ name: '文本文件', extensions: ['txt'] }],
      properties: count === 2 ? ['openFile', 'multiSelections'] : ['openFile']
    })
    return result.canceled ? [] : result.filePaths.slice(0, count === 2 ? 2 : 1)
  })
  ipcMain.handle('output:pick', async () => {
    const result = await dialog.showOpenDialog(windowRef, {
      title: '选择结果保存文件夹',
      properties: ['openDirectory', 'createDirectory']
    })
    return result.canceled ? '' : result.filePaths[0]
  })
  ipcMain.handle('output:open', async (_event, filePath) => {
    if (typeof filePath !== 'string' || !fs.existsSync(filePath)) return false
    await shell.openPath(filePath)
    return true
  })
  ipcMain.handle('analysis:run', async (_event, request) => {
    try {
      const input = request?.input
      const input2 = request?.input2
      if (typeof input !== 'string' || !input.toLowerCase().endsWith('.txt') || !fs.existsSync(input)) {
        throw new Error('请先选择有效的 TXT 文件。')
      }
      if (input2 && (typeof input2 !== 'string' || !input2.toLowerCase().endsWith('.txt') || !fs.existsSync(input2))) {
        throw new Error('第二份文件必须是有效的 TXT 文件。')
      }
      const metrics = Array.isArray(request.metrics) ? request.metrics.filter(x => allowedMetrics.has(x)) : []
      if (metrics.includes('crosscorr_dist') && !input2) throw new Error('互相关需要两份真值表 TXT。')
      if (input2 && !metrics.includes('crosscorr_dist')) throw new Error('双文件模式请选择互相关指标。')
      const kind = ['auto', 'truth', 'anf'].includes(request.kind) ? request.kind : 'auto'
      const radix = ['auto', 'bin', 'hex'].includes(request.radix) ? request.radix : 'auto'
      const args = ['--input', input, '--kind', kind, '--radix', radix, '--metrics', metrics.join(',')]
      if (input2) args.push('--input2', input2)
      if (request.n && Number.isInteger(Number(request.n))) args.push('--n', String(request.n))
      if (request.pcK && Number.isInteger(Number(request.pcK))) args.push('--pc-k', String(request.pcK))
      if (request.outputDir) args.push('--out', String(request.outputDir))
      if (metrics.includes('ea')) {
        if (request.eaRows) args.push('--ea-rows', String(request.eaRows).replace(/\s+/g, ''))
        if (request.eaAlpha) args.push('--ea-alpha', String(request.eaAlpha).replace(/\s+/g, ''))
        if (request.eaBeta) args.push('--ea-beta', String(request.eaBeta).replace(/\s+/g, ''))
        args.push('--ea-epsilon', String(request.eaEpsilon === '1' ? 1 : 0))
      }
      const executable = corePath()
      if (!fs.existsSync(executable)) throw new Error(`C++ 计算程序不存在：${executable}`)
      return await new Promise((resolve, reject) => {
        execFile(executable, args, { windowsHide: true, maxBuffer: 256 * 1024 * 1024, timeout: 300000 }, (err, stdout, stderr) => {
          try {
            const data = JSON.parse(stdout)
            resolve(data)
          } catch {
            reject(new Error((stderr || err?.message || stdout || '计算失败').trim()))
          }
        })
      })
    } catch (error) {
      return { ok: false, error: error.message }
    }
  })
  ipcMain.handle('vector:analysis:run', async (_event, request) => {
    try {
      if (typeof request?.input !== 'string' || !request.input.toLowerCase().endsWith('.txt') || !fs.existsSync(request.input)) throw new Error('请选择有效的 TXT 输入文件。')
      const n = Number(request.n), m = Number(request.m)
      if (!Number.isInteger(n) || n < 1 || n > 12 || !Number.isInteger(m) || m < 1 || m > Math.min(n, 8)) throw new Error('输入位数 n 须为 1–12，输出位数 m 须为 1–min(n,8)。')
      if (!['bin', 'hex'].includes(request.radix)) throw new Error('请选择输入进制。')
      const metrics = Array.isArray(request.metrics) ? request.metrics.filter(id => allowedVectorMetrics.has(id)) : []
      const args = ['--input', request.input, '--n', String(n), '--m', String(m), '--radix', request.radix, '--metrics', metrics.join(',')]
      if (request.transpose) args.push('--transpose')
      if (request.ddt) args.push('--ddt')
      if (request.lat) args.push('--lat')
      if (request.outputDir) args.push('--out', String(request.outputDir))
      const executable = vectorCorePath()
      if (!fs.existsSync(executable)) throw new Error(`C++ 计算程序不存在：${executable}`)
      return await new Promise((resolve, reject) => {
        execFile(executable, args, { windowsHide: true, maxBuffer: 16 * 1024 * 1024, timeout: 300000 }, (error, stdout, stderr) => {
          try { resolve(JSON.parse(stdout)) }
          catch { reject(new Error((stderr || error?.message || stdout || '计算失败').trim())) }
        })
      })
    } catch (error) { return { ok: false, error: error.message } }
  })
  createWindow()
})

app.on('window-all-closed', () => app.quit())
