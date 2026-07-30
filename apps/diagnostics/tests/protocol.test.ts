import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import { z } from "zod";

import {
  corruptCrc,
  decodeFrame,
  demoDeviceId,
  demoPrivateKey,
  encodeFrame,
  messageType,
  protocol,
  stableJson,
  toHex,
} from "../src/protocol.js";

const goldenSchema = z
  .object({
    protocolVersion: z.literal(1),
    unsignedTelemetry: z
      .object({
        deviceIdHex: z.string(),
        sequence: z.number().int(),
        monotonicMs: z.number().int(),
        payload: z.record(z.string(), z.unknown()),
        payloadUtf8: z.string(),
        bodyLength: z.number().int(),
        totalBytes: z.number().int(),
        crc32Hex: z.string(),
        frameHex: z.string(),
      })
      .passthrough(),
  })
  .passthrough();

test("TypeScript encoder reproduces the checked-in C++ golden vector", async () => {
  const path = new URL("../../../docs/protocol/golden-vectors.json", import.meta.url);
  const golden = goldenSchema.parse(JSON.parse(await readFile(path, "utf8")));
  const vector = golden.unsignedTelemetry;
  const frame = encodeFrame({
    type: messageType.telemetry,
    deviceId: Buffer.from(vector.deviceIdHex, "hex"),
    sequence: BigInt(vector.sequence),
    monotonicMs: BigInt(vector.monotonicMs),
    payload: vector.payload,
  });
  assert.equal(stableJson(vector.payload), vector.payloadUtf8);
  assert.equal(toHex(frame), vector.frameHex);

  const decoded = decodeFrame(frame);
  assert.equal(decoded.bodyLength, vector.bodyLength);
  assert.equal(decoded.totalBytes, vector.totalBytes);
  assert.equal(decoded.crc32Hex, vector.crc32Hex);
  assert.equal(decoded.crcValid, true);
  assert.equal(decoded.magic, "ST");
  assert.equal(decoded.messageType, messageType.telemetry);
});

test("signed frames have bounded lengths and an independently checked CRC", () => {
  const frame = encodeFrame(
    {
      type: messageType.command,
      deviceId: demoDeviceId(),
      sequence: 41n,
      monotonicMs: 9_001n,
      payload: { action: "request_status", commandId: "test-41" },
      authenticated: true,
    },
    demoPrivateKey(),
  );
  const decoded = decodeFrame(frame);
  assert.equal(decoded.signatureHex?.length, protocol.signatureBytes * 2);
  assert.equal(decoded.sequence, 41n);
  assert.equal(decoded.monotonicMs, 9_001n);
  assert.equal(decoded.crcValid, true);
  assert.equal(decodeFrame(corruptCrc(frame)).crcValid, false);
});

test("encoder and decoder reject invalid bounds and ambiguous authentication", () => {
  assert.throws(
    () =>
      encodeFrame({
        type: messageType.command,
        deviceId: new Uint8Array(15),
        sequence: 1n,
        monotonicMs: 1n,
        payload: {},
      }),
    /16 bytes/,
  );
  assert.throws(
    () =>
      encodeFrame({
        type: messageType.command,
        deviceId: demoDeviceId(),
        sequence: 1n,
        monotonicMs: 1n,
        payload: { value: "x".repeat(protocol.maxPayloadBytes + 1) },
      }),
    /payload exceeds/,
  );
  assert.throws(
    () =>
      encodeFrame({
        type: messageType.command,
        deviceId: demoDeviceId(),
        sequence: 1n,
        monotonicMs: 1n,
        payload: {},
        authenticated: true,
      }),
    /exactly one private key/,
  );
  assert.throws(() => decodeFrame("00"), /truncated/);
  assert.throws(() => decodeFrame("xyz"), /even-length hexadecimal/);
});
