import {
  createPrivateKey,
  sign as signDetached,
  type KeyObject,
} from "node:crypto";

export const protocol = {
  version: 1,
  lengthPrefixBytes: 4,
  headerBytes: 44,
  crcBytes: 4,
  signatureBytes: 64,
  maxPayloadBytes: 4096,
  maxFrameBytes: 4212,
  authenticatedFlag: 0x01,
} as const;

export const messageType = {
  registration: 1,
  configuration: 2,
  command: 3,
  telemetry: 4,
  acknowledgement: 5,
  error: 6,
} as const;

export type MessageType = (typeof messageType)[keyof typeof messageType];

export interface FrameInput {
  type: MessageType;
  deviceId: Uint8Array;
  sequence: bigint;
  monotonicMs: bigint;
  payload: unknown;
  authenticated?: boolean;
}

export interface DecodedFrame {
  bodyLength: number;
  magic: string;
  version: number;
  headerLength: number;
  messageType: number;
  flags: number;
  errorCode: number;
  deviceIdHex: string;
  sequence: bigint;
  monotonicMs: bigint;
  payloadLength: number;
  payloadUtf8: string;
  signatureHex?: string;
  crc32Hex: string;
  crcValid: boolean;
  totalBytes: number;
}

const encoder = new TextEncoder();
const decoder = new TextDecoder("utf-8", { fatal: true });
const authenticationDomain = encoder.encode("SourceTraceEdge/v1\0");
const demoPkcs8Prefix = Buffer.from("302e020100300506032b657004220420", "hex");

export function demoDeviceId(): Uint8Array {
  return Uint8Array.from({ length: 16 }, (_value, index) => index);
}

export function demoPrivateKey(): KeyObject {
  const seed = Buffer.from(Uint8Array.from({ length: 32 }, (_value, index) => index));
  return createPrivateKey({
    key: Buffer.concat([demoPkcs8Prefix, seed]),
    format: "der",
    type: "pkcs8",
  });
}

export function stableJson(value: unknown): string {
  if (Array.isArray(value)) {
    return `[${value.map((item) => stableJson(item)).join(",")}]`;
  }
  if (value !== null && typeof value === "object") {
    const record = value as Record<string, unknown>;
    return `{${Object.keys(record)
      .sort()
      .map((key) => `${JSON.stringify(key)}:${stableJson(record[key])}`)
      .join(",")}}`;
  }
  const serialized = JSON.stringify(value);
  if (serialized === undefined) {
    throw new TypeError("protocol payload contains a non-JSON value");
  }
  return serialized;
}

function writeHeaderAndPayload(input: FrameInput, payloadBytes: Uint8Array): Buffer {
  if (input.deviceId.length !== 16) {
    throw new RangeError("device identifier must be 16 bytes");
  }
  if (payloadBytes.length > protocol.maxPayloadBytes) {
    throw new RangeError("payload exceeds protocol maximum");
  }
  const header = Buffer.alloc(protocol.headerBytes + payloadBytes.length);
  const view = new DataView(header.buffer, header.byteOffset, header.byteLength);
  header.write("ST", 0, "ascii");
  view.setUint8(2, protocol.version);
  view.setUint8(3, protocol.headerBytes);
  view.setUint8(4, input.type);
  view.setUint8(5, input.authenticated === true ? protocol.authenticatedFlag : 0);
  view.setUint16(6, 0, false);
  header.set(input.deviceId, 8);
  view.setBigUint64(24, input.sequence, false);
  view.setBigUint64(32, input.monotonicMs, false);
  view.setUint32(40, payloadBytes.length, false);
  header.set(payloadBytes, protocol.headerBytes);
  return header;
}

export function encodeFrame(input: FrameInput, privateKey?: KeyObject): Buffer {
  const payloadBytes = encoder.encode(stableJson(input.payload));
  const authenticated = input.authenticated === true;
  if (authenticated !== (privateKey !== undefined)) {
    throw new Error("authenticated frames require exactly one private key");
  }
  const headerAndPayload = writeHeaderAndPayload(input, payloadBytes);
  const signature =
    authenticated && privateKey !== undefined
      ? signDetached(
          null,
          Buffer.concat([Buffer.from(authenticationDomain), headerAndPayload]),
          privateKey,
        )
      : Buffer.alloc(0);
  if (signature.length !== (authenticated ? protocol.signatureBytes : 0)) {
    throw new Error("unexpected Ed25519 signature length");
  }
  const protectedBody = Buffer.concat([headerAndPayload, signature]);
  const body = Buffer.alloc(protectedBody.length + protocol.crcBytes);
  protectedBody.copy(body);
  new DataView(body.buffer, body.byteOffset, body.byteLength).setUint32(
    protectedBody.length,
    crc32(protectedBody),
    false,
  );
  const frame = Buffer.alloc(protocol.lengthPrefixBytes + body.length);
  new DataView(frame.buffer, frame.byteOffset, frame.byteLength).setUint32(
    0,
    body.length,
    false,
  );
  body.copy(frame, protocol.lengthPrefixBytes);
  if (frame.length > protocol.maxFrameBytes) {
    throw new RangeError("encoded frame exceeds protocol maximum");
  }
  return frame;
}

export function decodeFrame(input: Uint8Array | string): DecodedFrame {
  const bytes =
    typeof input === "string"
      ? Buffer.from(validateHex(input), "hex")
      : Buffer.from(input.buffer, input.byteOffset, input.byteLength);
  if (bytes.length < protocol.lengthPrefixBytes + protocol.headerBytes + protocol.crcBytes) {
    throw new RangeError("frame is truncated");
  }
  if (bytes.length > protocol.maxFrameBytes) {
    throw new RangeError("frame exceeds protocol maximum");
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const bodyLength = view.getUint32(0, false);
  if (bodyLength !== bytes.length - protocol.lengthPrefixBytes) {
    throw new RangeError("outer length does not match available bytes");
  }
  const version = view.getUint8(6);
  const headerLength = view.getUint8(7);
  const flags = view.getUint8(9);
  const payloadLength = view.getUint32(44, false);
  if (headerLength !== protocol.headerBytes || payloadLength > protocol.maxPayloadBytes) {
    throw new RangeError("header or payload length is outside v1 bounds");
  }
  const signatureLength =
    (flags & protocol.authenticatedFlag) !== 0 ? protocol.signatureBytes : 0;
  const expected =
    protocol.lengthPrefixBytes +
    protocol.headerBytes +
    payloadLength +
    signatureLength +
    protocol.crcBytes;
  if (bytes.length !== expected) {
    throw new RangeError("inner lengths do not match the frame");
  }

  const payloadStart = protocol.lengthPrefixBytes + protocol.headerBytes;
  const signatureStart = payloadStart + payloadLength;
  const crcOffset = bytes.length - protocol.crcBytes;
  const suppliedCrc = view.getUint32(crcOffset, false);
  const protectedBody = bytes.subarray(protocol.lengthPrefixBytes, crcOffset);
  const signature =
    signatureLength === 0 ? undefined : bytes.subarray(signatureStart, crcOffset);
  return {
    bodyLength,
    magic: bytes.subarray(4, 6).toString("ascii"),
    version,
    headerLength,
    messageType: view.getUint8(8),
    flags,
    errorCode: view.getUint16(10, false),
    deviceIdHex: bytes.subarray(12, 28).toString("hex"),
    sequence: view.getBigUint64(28, false),
    monotonicMs: view.getBigUint64(36, false),
    payloadLength,
    payloadUtf8: decoder.decode(bytes.subarray(payloadStart, signatureStart)),
    ...(signature === undefined ? {} : { signatureHex: signature.toString("hex") }),
    crc32Hex: suppliedCrc.toString(16).padStart(8, "0"),
    crcValid: suppliedCrc === crc32(protectedBody),
    totalBytes: bytes.length,
  };
}

export function crc32(input: Uint8Array): number {
  let crc = 0xffff_ffff;
  for (const value of input) {
    crc ^= value;
    for (let bit = 0; bit < 8; bit += 1) {
      crc = (crc >>> 1) ^ (crc & 1 ? 0xedb8_8320 : 0);
    }
  }
  return (crc ^ 0xffff_ffff) >>> 0;
}

export function toHex(input: Uint8Array): string {
  return Buffer.from(input.buffer, input.byteOffset, input.byteLength).toString("hex");
}

export function corruptCrc(frame: Uint8Array): Buffer {
  const corrupted = Buffer.from(frame);
  corrupted[corrupted.length - 1] = (corrupted.at(-1) ?? 0) ^ 0x80;
  return corrupted;
}

function validateHex(value: string): string {
  if (value.length === 0 || value.length % 2 !== 0 || !/^[0-9a-f]+$/i.test(value)) {
    throw new TypeError("frame must be non-empty even-length hexadecimal");
  }
  return value;
}
