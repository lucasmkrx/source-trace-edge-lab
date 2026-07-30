import {
  corruptFinalCrcBit,
  decodeFrameHex,
  goldenFrameHex,
  scenarioSteps,
} from "./model.mjs";

const elements = {
  run: document.querySelector("#run-scenario"),
  reset: document.querySelector("#reset-scenario"),
  list: document.querySelector("#trace-list"),
  counter: document.querySelector("#step-counter"),
  mode: document.querySelector("#state-mode"),
  health: document.querySelector("#state-health"),
  connected: document.querySelector("#state-connected"),
  queue: document.querySelector("#state-queue"),
  effects: document.querySelector("#state-effects"),
  outcome: document.querySelector("#state-outcome"),
  indicator: document.querySelector("#state-indicator"),
  frameHex: document.querySelector("#frame-hex"),
  decode: document.querySelector("#decode-frame"),
  corrupt: document.querySelector("#corrupt-frame"),
  protocolOutput: document.querySelector("#protocol-output"),
};

let scenarioTimer;
let scenarioIndex = 0;

elements.frameHex.value = goldenFrameHex;
elements.run.addEventListener("click", runScenario);
elements.reset.addEventListener("click", resetScenario);
elements.decode.addEventListener("click", renderDecodedFrame);
elements.corrupt.addEventListener("click", () => {
  try {
    elements.frameHex.value = corruptFinalCrcBit(elements.frameHex.value);
    renderDecodedFrame();
  } catch (error) {
    renderDecodeError(error);
  }
});

function runScenario() {
  resetScenario();
  elements.run.disabled = true;
  advanceScenario();
  scenarioTimer = window.setInterval(() => {
    if (scenarioIndex >= scenarioSteps.length) {
      window.clearInterval(scenarioTimer);
      elements.run.disabled = false;
      return;
    }
    advanceScenario();
  }, prefersReducedMotion() ? 80 : 430);
}

function advanceScenario() {
  const step = scenarioSteps[scenarioIndex];
  if (step === undefined) {
    return;
  }
  scenarioIndex += 1;
  renderState(step);
  const item = document.createElement("li");
  const badgeClass = step.accepted ? "" : " rejected";
  item.innerHTML = `
    <span class="trace-index">${String(scenarioIndex).padStart(2, "0")}</span>
    <span class="trace-copy">
      <strong>${escapeHtml(step.label)}</strong>
      <span>${escapeHtml(step.detail)}</span>
    </span>
    <span class="trace-badge${badgeClass}">${escapeHtml(step.badge ?? "accepted")}</span>
  `;
  elements.list.append(item);
  item.scrollIntoView({ block: "nearest", behavior: prefersReducedMotion() ? "auto" : "smooth" });
}

function renderState(step) {
  elements.counter.textContent = `step ${scenarioIndex} / ${scenarioSteps.length}`;
  elements.mode.textContent = step.mode;
  elements.health.textContent = step.health;
  elements.connected.textContent = String(step.connected);
  elements.queue.textContent = `${step.queue} ${step.queue === 1 ? "record" : "records"}`;
  elements.effects.textContent = String(step.effects);
  elements.outcome.textContent = step.outcome;
  elements.indicator.className = "state-indicator";
  if (!step.accepted) {
    elements.indicator.classList.add("rejected");
  } else if (step.health === "nominal") {
    elements.indicator.classList.add("active");
  }
}

function resetScenario() {
  window.clearInterval(scenarioTimer);
  scenarioIndex = 0;
  elements.run.disabled = false;
  elements.list.replaceChildren();
  elements.counter.textContent = `step 0 / ${scenarioSteps.length}`;
  elements.mode.textContent = "booting";
  elements.health.textContent = "network_degraded";
  elements.connected.textContent = "false";
  elements.queue.textContent = "0 records";
  elements.effects.textContent = "0";
  elements.outcome.textContent = "Ready to replay evidence.";
  elements.indicator.className = "state-indicator";
}

function renderDecodedFrame() {
  try {
    const frame = decodeFrameHex(elements.frameHex.value);
    let parsedPayload = frame.payloadText;
    try {
      parsedPayload = JSON.stringify(JSON.parse(frame.payloadText), null, 2);
    } catch {
      // Raw UTF-8 is still useful when the payload is not JSON.
    }
    elements.protocolOutput.innerHTML = `
      <div class="decode-status">
        <strong>${escapeHtml(frame.typeName)} frame</strong>
        <span class="${frame.crcValid ? "valid" : "invalid"}">
          CRC ${frame.crcValid ? "VALID" : "MISMATCH"}
        </span>
      </div>
      <div class="decode-grid">
        ${field("Magic", frame.magic)}
        ${field("Version / header", `${frame.version} / ${frame.headerLength} bytes`)}
        ${field("Message type", `${frame.type} · ${frame.typeName}`)}
        ${field("Flags / error", `0x${frame.flags.toString(16).padStart(2, "0")} / ${frame.errorCode}`)}
        ${field("Sequence", frame.sequence.toString())}
        ${field("Monotonic ms", frame.monotonicMs.toString())}
        ${field("Payload", `${frame.payloadLength} bytes`)}
        ${field("Frame", `${frame.totalBytes} bytes`)}
        ${field("Supplied CRC32", frame.suppliedCrc)}
        ${field("Calculated CRC32", frame.calculatedCrc)}
      </div>
      <div class="decode-payload">
        <small>Device identifier</small>
        <code>${escapeHtml(frame.deviceId)}</code>
      </div>
      <div class="decode-payload">
        <small>UTF-8 payload</small>
        <pre>${escapeHtml(parsedPayload)}</pre>
      </div>
    `;
  } catch (error) {
    renderDecodeError(error);
  }
}

function renderDecodeError(error) {
  const message = error instanceof Error ? error.message : String(error);
  elements.protocolOutput.innerHTML = `
    <p class="decode-error"><strong>Decoder rejected input</strong></p>
    <p class="output-placeholder">${escapeHtml(message)}</p>
  `;
}

function field(label, value) {
  return `<div><small>${escapeHtml(label)}</small><code>${escapeHtml(value)}</code></div>`;
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function prefersReducedMotion() {
  return window.matchMedia("(prefers-reduced-motion: reduce)").matches;
}
