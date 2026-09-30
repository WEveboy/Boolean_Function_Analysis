const { contextBridge, ipcRenderer, webUtils } = require('electron')

contextBridge.exposeInMainWorld('vbfDesktop', {
  pickInput: () => ipcRenderer.invoke('input:pick'),
  pickOutput: () => ipcRenderer.invoke('output:pick'),
  pathForFile: (file) => webUtils.getPathForFile(file),
  analyze: (request) => ipcRenderer.invoke('analysis:run', request),
  openFile: (filePath) => ipcRenderer.invoke('file:open', filePath)
})
