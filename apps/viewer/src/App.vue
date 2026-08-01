<script setup lang="ts">
import { ref, onMounted } from 'vue'

const isNetworkOn = ref(false)
const isOccupied = ref(false)
const isBooted = ref(false)
const queueDepth = ref(0)
const logs = ref<{ time: string, message: string }[]>([])

// TS definition for our exposed API
declare global {
  interface Window {
    electronAPI: {
      sendRequest: (request: any) => Promise<any>
      onResponse: (callback: (data: any) => void) => void
    }
  }
}

onMounted(() => {
  if (window.electronAPI) {
    window.electronAPI.onResponse((data: any) => {
      const now = new Date().toLocaleTimeString()
      
      // Look for side-effects in the command response
      if (data.status === 'ok') {
        logs.value.unshift({ time: now, message: `Success: ${JSON.stringify(data.events || [])}` })
      } else if (data.status === 'error') {
        logs.value.unshift({ time: now, message: `Error: ${data.reason}` })
      } else {
        logs.value.unshift({ time: now, message: `Received: ${JSON.stringify(data)}` })
      }
      
      if (logs.value.length > 50) logs.value.pop()
    })
  } else {
    logs.value.unshift({ time: new Date().toLocaleTimeString(), message: 'Electron API not found' })
  }
})

let seq = 1
let nowMs = 100

function logAction(msg: string) {
  logs.value.unshift({ time: new Date().toLocaleTimeString(), message: `Command: ${msg}` })
}

async function sendBoot() {
  isBooted.value = true
  logAction('boot')
  await window.electronAPI?.sendRequest({ action: 'boot', atMs: nowMs++ })
}

async function sendNetwork() {
  isNetworkOn.value = !isNetworkOn.value
  logAction(`network ${isNetworkOn.value ? 'on' : 'off'}`)
  await window.electronAPI?.sendRequest({ action: 'network', atMs: nowMs++, connected: isNetworkOn.value })
}

async function sendOccupancy() {
  isOccupied.value = !isOccupied.value
  logAction(`occupancy ${isOccupied.value ? 'on' : 'off'}`)
  await window.electronAPI?.sendRequest({ action: 'occupancy', atMs: nowMs++, occupied: isOccupied.value })
}

async function sendButton() {
  logAction('button pressed')
  await window.electronAPI?.sendRequest({ action: 'button', atMs: nowMs++ })
}
</script>

<template>
  <div class="dashboard">
    <header class="header">
      <h1>SourceTrace Edge Terminal</h1>
      <div class="status-indicator" :class="{ active: isBooted }">
        {{ isBooted ? 'System Online' : 'System Offline' }}
      </div>
    </header>

    <main class="grid">
      <section class="panel controls">
        <h2>Hardware Controls</h2>
        <div class="button-group">
          <button @click="sendBoot" :disabled="isBooted" class="btn primary">Initialize Boot</button>
          <button @click="sendNetwork" class="btn" :class="{ active: isNetworkOn }">
            Network: {{ isNetworkOn ? 'ON' : 'OFF' }}
          </button>
          <button @click="sendOccupancy" class="btn" :class="{ active: isOccupied }">
            Occupancy: {{ isOccupied ? 'DETECTED' : 'CLEAR' }}
          </button>
          <button @click="sendButton" class="btn warning">Hardware Button</button>
        </div>
      </section>

      <section class="panel telemetry">
        <h2>Telemetry Log</h2>
        <div class="log-container">
          <div v-for="(log, index) in logs" :key="index" class="log-entry">
            <span class="time">[{{ log.time }}]</span>
            <span class="message">{{ log.message }}</span>
          </div>
          <div v-if="logs.length === 0" class="empty-log">Awaiting events...</div>
        </div>
      </section>
    </main>
  </div>
</template>
