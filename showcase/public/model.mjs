export const goldenFrameHex =
  "000000845354012c04000000000102030405060708090a0b0c0d0e0f" +
  "00000000000000010000000000000002000000547b226576656e744964223a" +
  "226576742d31222c226865616c7468223a226e6f6d696e616c222c226d6f64" +
  "65223a2269646c65222c226f63637570696564223a66616c73652c22717565" +
  "75654465707468223a307d06125e47";

export const scenarioSteps = Object.freeze([
  {
    label: "Initial status",
    detail: "Fresh runtime, no durable work",
    mode: "booting",
    health: "network_degraded",
    connected: false,
    queue: 0,
    effects: 0,
    outcome: "Status observed without changing state.",
    accepted: true,
  },
  {
    label: "Complete boot",
    detail: "Explicit monotonic event at 1 ms",
    mode: "idle",
    health: "network_degraded",
    connected: false,
    queue: 0,
    effects: 0,
    outcome: "Boot completed; offline health remains degraded.",
    accepted: true,
  },
  {
    label: "Signed configuration",
    detail: "Ed25519 sequence 7 accepted",
    mode: "idle",
    health: "network_degraded",
    connected: false,
    queue: 1,
    effects: 0,
    outcome: "Configuration, replay record, audit, and telemetry committed.",
    accepted: true,
  },
  {
    label: "Occupancy observed",
    detail: "Local input while offline",
    mode: "idle",
    health: "network_degraded",
    connected: false,
    queue: 2,
    effects: 0,
    outcome: "Occupancy state and outbound telemetry committed locally.",
    accepted: true,
  },
  {
    label: "Signed session start",
    detail: "Authenticated sequence 8",
    mode: "session_active",
    health: "network_degraded",
    connected: false,
    queue: 3,
    effects: 0,
    outcome: "Session starts locally despite network loss.",
    accepted: true,
  },
  {
    label: "Process restart",
    detail: "Reconstruct from SQLite",
    mode: "session_active",
    health: "network_degraded",
    connected: false,
    queue: 3,
    effects: 0,
    outcome: "Active session, queue, sequence, and replay state survive restart.",
    accepted: true,
  },
  {
    label: "Identical replay",
    detail: "Same sequence and signature",
    mode: "session_active",
    health: "network_degraded",
    connected: false,
    queue: 3,
    effects: 0,
    outcome: "Duplicate acknowledged without repeating the domain effect.",
    accepted: true,
    badge: "duplicate",
  },
  {
    label: "Sequence conflict",
    detail: "Sequence 8, different signed content",
    mode: "session_active",
    health: "network_degraded",
    connected: false,
    queue: 3,
    effects: 0,
    outcome: "Authenticated equivocation rejected as sequence_conflict.",
    accepted: false,
    badge: "rejected",
  },
  {
    label: "CRC corruption",
    detail: "Final integrity bit flipped",
    mode: "session_active",
    health: "network_degraded",
    connected: false,
    queue: 3,
    effects: 0,
    outcome: "Frame rejected before application handling with protocol error 9.",
    accepted: false,
    badge: "rejected",
  },
  {
    label: "Arm ACK fault",
    detail: "One local acknowledgement failure",
    mode: "session_active",
    health: "network_degraded",
    connected: false,
    queue: 3,
    effects: 0,
    outcome: "Deterministic storage fault injection armed.",
    accepted: true,
  },
  {
    label: "Connect and deliver",
    detail: "Receiver applies; local ACK fails",
    mode: "session_active",
    health: "nominal",
    connected: true,
    queue: 4,
    effects: 1,
    outcome: "Remote effect occurs, but durable queue record remains for recovery.",
    accepted: false,
    badge: "storage fault",
  },
  {
    label: "Recovery flush",
    detail: "Retransmit with stable event ID",
    mode: "session_active",
    health: "nominal",
    connected: true,
    queue: 0,
    effects: 4,
    outcome: "Four records drain; duplicate transmission adds no receiver effect.",
    accepted: true,
  },
  {
    label: "Network lost",
    detail: "Queue one transition offline",
    mode: "session_active",
    health: "network_degraded",
    connected: false,
    queue: 1,
    effects: 4,
    outcome: "Local session continues and connectivity telemetry is retained.",
    accepted: true,
  },
  {
    label: "Second restart",
    detail: "Offline queue reconstructed",
    mode: "session_active",
    health: "network_degraded",
    connected: false,
    queue: 1,
    effects: 4,
    outcome: "Pending frame and degraded active state survive another process.",
    accepted: true,
  },
  {
    label: "Reconnect and drain",
    detail: "New bounded delivery opportunity",
    mode: "session_active",
    health: "nominal",
    connected: true,
    queue: 0,
    effects: 6,
    outcome: "Queue drains in order; final receiver effect count is six.",
    accepted: true,
  },
]);

const protocol = Object.freeze({
  maxFrameBytes: 4212,
  headerBytes: 44,
  crcBytes: 4,
  signatureBytes: 64,
  authenticatedFlag: 0x01,
});

const messageNames = Object.freeze({
  1: "registration",
  2: "configuration",
  3: "command",
  4: "telemetry",
  5: "acknowledgement",
  6: "error",
});

export function decodeFrameHex(value) {
  const normalized = value.replaceAll(/\s/g, "");
  if (normalized.length === 0 || normalized.length % 2 !== 0) {
    throw new TypeError("Frame must contain non-empty, even-length hexadecimal.");
  }
  if (!/^[0-9a-f]+$/i.test(normalized)) {
    throw new TypeError("Frame contains a non-hexadecimal character.");
  }
  const bytes = Uint8Array.from(normalized.match(/../g), (byte) => Number.parseInt(byte, 16));
  if (bytes.length < 52 || bytes.length > protocol.maxFrameBytes) {
    throw new RangeError("Frame is outside the 52–4212 byte decoder bound.");
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const bodyLength = view.getUint32(0, false);
  if (bodyLength !== bytes.length - 4) {
    throw new RangeError("Outer body length does not match available bytes.");
  }
  const headerLength = view.getUint8(7);
  const flags = view.getUint8(9);
  const payloadLength = view.getUint32(44, false);
  if (headerLength !== protocol.headerBytes || payloadLength > 4096) {
    throw new RangeError("Header or payload length violates protocol v1.");
  }
  const signatureLength =
    (flags & protocol.authenticatedFlag) === 0 ? 0 : protocol.signatureBytes;
  const expected = 4 + protocol.headerBytes + payloadLength + signatureLength + 4;
  if (bytes.length !== expected) {
    throw new RangeError("Inner lengths do not match the complete frame.");
  }
  const payloadStart = 4 + protocol.headerBytes;
  const payloadEnd = payloadStart + payloadLength;
  const crcOffset = bytes.length - protocol.crcBytes;
  const suppliedCrc = view.getUint32(crcOffset, false);
  const calculatedCrc = crc32(bytes.subarray(4, crcOffset));
  const type = view.getUint8(8);
  const payloadText = new TextDecoder("utf-8", { fatal: true }).decode(
    bytes.subarray(payloadStart, payloadEnd),
  );

  return {
    totalBytes: bytes.length,
    bodyLength,
    magic: String.fromCharCode(bytes[4], bytes[5]),
    version: view.getUint8(6),
    headerLength,
    type,
    typeName: messageNames[type] ?? "unknown",
    flags,
    errorCode: view.getUint16(10, false),
    deviceId: toHex(bytes.subarray(12, 28)),
    sequence: view.getBigUint64(28, false),
    monotonicMs: view.getBigUint64(36, false),
    payloadLength,
    payloadText,
    suppliedCrc: hex32(suppliedCrc),
    calculatedCrc: hex32(calculatedCrc),
    crcValid: suppliedCrc === calculatedCrc,
  };
}

export function corruptFinalCrcBit(value) {
  const normalized = value.replaceAll(/\s/g, "");
  if (normalized.length < 2 || !/^[0-9a-f]+$/i.test(normalized)) {
    throw new TypeError("Load valid hexadecimal before corrupting the CRC.");
  }
  const finalByte = Number.parseInt(normalized.slice(-2), 16) ^ 0x80;
  return `${normalized.slice(0, -2)}${finalByte.toString(16).padStart(2, "0")}`;
}

export function crc32(bytes) {
  let crc = 0xffff_ffff;
  for (const value of bytes) {
    crc ^= value;
    for (let bit = 0; bit < 8; bit += 1) {
      crc = (crc >>> 1) ^ (crc & 1 ? 0xedb8_8320 : 0);
    }
  }
  return (crc ^ 0xffff_ffff) >>> 0;
}

function toHex(bytes) {
  return [...bytes].map((value) => value.toString(16).padStart(2, "0")).join("");
}

function hex32(value) {
  return value.toString(16).padStart(8, "0");
}
