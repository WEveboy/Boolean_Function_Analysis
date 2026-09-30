const { app, BrowserWindow, dialog, ipcMain, shell } = require('electron')
const path = require('node:path')
const fs = require('node:fs')
const { execFile } = require('node:child_process')

const allowed = new Set([
  'balance', 'max_degree', 'min_degree', 'ai', 'nonlinearity', 'resiliency',
  'bent', 'almost_optimal', 'diff_uniformity', 'diff_probability',
  'diff_deviation', 'diff_stddev', 'pn', 'apn', 'linearity',
  'linear_bias', 'linear_probability', 'ab'
])
let mainWindow
function corePath() {
  return app.isPackaged
    ? path.join(process.resourcesPath, 'bin', 'vbf-core.exe')
    : path.resolve(__dirname, '../../cpp/build/vbf-core.exe')
}
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1460, height: 930, minWidth: 980, minHeight: 680,
    title: '多输出布尔函数安全性分析', backgroundColor: '#f4f6fa',
    autoHideMenuBar: true,
    webPreferences: { preload: path.join(__dirname, 'preload.cjs'), contextIsolation: true, nodeIntegration: false, sandbox: true }
  })
  mainWindow.webContents.setWindowOpenHandler(() => ({ action: 'deny' }))
  if (!app.isPackaged && process.env.VBF_DEV_URL) mainWindow.loadURL(process.env.VBF_DEV_URL)
  else mainWindow.loadFile(path.join(__dirname, '../dist/index.html'))
  if (process.env.VBF_SMOKE_PATH) {
    mainWindow.webContents.once('did-finish-load', () => setTimeout(async () => {
      try {
        const state = await mainWindow.webContents.executeJavaScript(`(async () => {
          const status = { title: document.title, heading: document.querySelector('h1')?.textContent,
            metricCount: document.querySelectorAll('.metric-tile').length,
            desktopApi: typeof window.vbfDesktop?.analyze === 'function' }
          ${process.env.VBF_SMOKE_INPUT ? `status.backend = await window.vbfDesktop.analyze({input:${JSON.stringify(process.env.VBF_SMOKE_INPUT)},n:4,m:4,radix:'hex',transpose:${process.env.VBF_SMOKE_TRANSPOSE === '1'},metrics:['balance','diff_uniformity','linearity'],ddt:true,lat:true,outputDir:${JSON.stringify(process.env.VBF_SMOKE_OUT || '')}})` : ''}
          return status
        })()`)
        fs.writeFileSync(process.env.VBF_SMOKE_PATH + '.json', JSON.stringify(state, null, 2))
        const image = await mainWindow.capturePage()
        fs.writeFileSync(process.env.VBF_SMOKE_PATH, image.toPNG())
      } catch (error) { fs.writeFileSync(process.env.VBF_SMOKE_PATH + '.error.txt', String(error.stack || error)) }
      finally { app.quit() }
    }, 1000))
  }
}
app.whenReady().then(() => {
  ipcMain.handle('input:pick', async () => {
    const r = await dialog.showOpenDialog(mainWindow, { title: '选择 S 盒或向量函数 TXT', filters: [{ name: '文本文件', extensions: ['txt'] }], properties: ['openFile'] })
    return r.canceled ? '' : r.filePaths[0]
  })
  ipcMain.handle('output:pick', async () => {
    const r = await dialog.showOpenDialog(mainWindow, { title: '选择输出文件夹', properties: ['openDirectory', 'createDirectory'] })
    return r.canceled ? '' : r.filePaths[0]
  })
  ipcMain.handle('file:open', async (_event, filePath) => {
    if (typeof filePath !== 'string' || !fs.existsSync(filePath)) return false
    return (await shell.openPath(filePath)) === ''
  })
  ipcMain.handle('analysis:run', async (_event, request) => {
    try {
      if (typeof request?.input !== 'string' || !request.input.toLowerCase().endsWith('.txt') || !fs.existsSync(request.input)) throw new Error('请选择有效的 TXT 输入文件。')
      const n = Number(request.n), m = Number(request.m)
      if (!Number.isInteger(n) || n < 1 || n > 12 || !Number.isInteger(m) || m < 1 || m > Math.min(n, 8)) throw new Error('输入位数 n 须为 1–12，输出位数 m 须为 1–min(n,8)。')
      if (!['bin', 'hex'].includes(request.radix)) throw new Error('请选择输入进制。')
      const metrics = Array.isArray(request.metrics) ? request.metrics.filter(id => allowed.has(id)) : []
      const args = ['--input', request.input, '--n', String(n), '--m', String(m), '--radix', request.radix, '--metrics', metrics.join(',')]
      if (request.transpose) args.push('--transpose')
      if (request.ddt) args.push('--ddt')
      if (request.lat) args.push('--lat')
      if (request.outputDir) args.push('--out', String(request.outputDir))
      const executable = corePath()
      if (!fs.existsSync(executable)) throw new Error('C++ 计算程序不存在。')
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
