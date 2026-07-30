import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

import {
  corruptFinalCrcBit,
  decodeFrameHex,
  goldenFrameHex,
  scenarioSteps,
} from "../public/model.mjs";

test("showcase golden frame matches the normative protocol fixture", async () => {
  const fixture = JSON.parse(
    await readFile(new URL("../../docs/protocol/golden-vectors.json", import.meta.url), "utf8"),
  );
  assert.equal(goldenFrameHex, fixture.unsignedTelemetry.frameHex);
  const decoded = decodeFrameHex(goldenFrameHex);
  assert.equal(decoded.magic, "ST");
  assert.equal(decoded.typeName, "telemetry");
  assert.equal(decoded.sequence, 1n);
  assert.equal(decoded.payloadLength, 84);
  assert.equal(decoded.crcValid, true);
});

test("browser decoder identifies corruption without calling it authentication", () => {
  const decoded = decodeFrameHex(corruptFinalCrcBit(goldenFrameHex));
  assert.equal(decoded.crcValid, false);
  assert.notEqual(decoded.suppliedCrc, decoded.calculatedCrc);
});

test("recorded showcase trace preserves the verified integration summary", () => {
  assert.equal(scenarioSteps.length, 15);
  const last = scenarioSteps.at(-1);
  assert.equal(last?.mode, "session_active");
  assert.equal(last?.health, "nominal");
  assert.equal(last?.queue, 0);
  assert.equal(last?.effects, 6);
  assert.equal(scenarioSteps.filter((step) => !step.accepted).length, 3);
});

test("public page labels the trace as recorded evidence and exposes no ingestion endpoint", async () => {
  const html = await readFile(new URL("../public/index.html", import.meta.url), "utf8");
  assert.match(html, /replays checked-in integration evidence/i);
  assert.match(html, /no simulator,\s+ingestion worker/i);
  assert.doesNotMatch(html, /\/v1\/ingest/);
  assert.doesNotMatch(html, /AIza[0-9A-Za-z_-]{30,}/);
});
