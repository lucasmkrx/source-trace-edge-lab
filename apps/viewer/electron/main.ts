import { app, BrowserWindow, ipcMain } from 'electron'
import { join } from 'node:path'
import { spawn } from 'node:child_process'
import { createInterface } from 'node:readline'

let win: BrowserWindow | null = null
let simulatorProcess: any = null

function createWindow() {
  win = new BrowserWindow({
    width: 1000,
    height: 700,
    webPreferences: {
      preload: join(__dirname, 'preload.mjs'),
      nodeIntegration: false,
      contextIsolation: true,
    },
    titleBarStyle: 'hiddenInset',
    backgroundColor: '#0f172a'
  })

  if (process.env.VITE_DEV_SERVER_URL) {
    win.loadURL(process.env.VITE_DEV_SERVER_URL)
  } else {
    win.loadFile(join(__dirname, '../dist/index.html'))
  }
}

app.whenReady().then(() => {
  createWindow()
  
  // Start the C++ simulator
  const simBinary = join(__dirname, '../../../../build/dev/edge/sourcetrace_sim')
  simulatorProcess = spawn(simBinary, [], {
    shell: false,
    stdio: ['pipe', 'pipe', 'pipe']
  })

  createInterface({ input: simulatorProcess.stdout }).on('line', (line) => {
    try {
      const data = JSON.parse(line)
      if (win) {
        win.webContents.send('simulator:response', data)
      }
    } catch (e) {
      console.error("Failed to parse sim output", e)
    }
  })

  simulatorProcess.stderr.on('data', (data: Buffer) => {
    console.error(`Sim Error: ${data.toString()}`)
  })
})

app.on('window-all-closed', () => {
  if (simulatorProcess) {
    simulatorProcess.kill('SIGKILL')
  }
  if (process.platform !== 'darwin') {
    app.quit()
  }
})

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow()
  }
})

ipcMain.handle('simulator:request', async (event, request) => {
  if (simulatorProcess) {
    simulatorProcess.stdin.write(JSON.stringify(request) + '\n')
  }
  return true
})
