import { contextBridge, ipcRenderer } from 'electron'

contextBridge.exposeInMainWorld('electronAPI', {
  sendRequest: (request: any) => ipcRenderer.invoke('simulator:request', request),
  onResponse: (callback: (data: any) => void) => {
    ipcRenderer.on('simulator:response', (_event, value) => callback(value))
  }
})
