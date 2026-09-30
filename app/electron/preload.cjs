const { contextBridge, ipcRenderer, webUtils } = require('electron')

contextBridge.exposeInMainWorld('bfDesktop', {
  pickFiles: (count) => ipcRenderer.invoke('files:pick', count),
  pickOutput: () => ipcRenderer.invoke('output:pick'),
  pathForFile: (file) => webUtils.getPathForFile(file),
  run: (request) => ipcRenderer.invoke('analysis:run', request),
  openOutput: (filePath) => ipcRenderer.invoke('output:open', filePath)
})

contextBridge.exposeInMainWorld('vbfDesktop', {
  pickInput: () => ipcRenderer.invoke('vector:input:pick'),
  pickOutput: () => ipcRenderer.invoke('output:pick'),
  pathForFile: (file) => webUtils.getPathForFile(file),
  analyze: (request) => ipcRenderer.invoke('vector:analysis:run', request),
  openFile: (filePath) => ipcRenderer.invoke('output:open', filePath)
})
