import assert from "node:assert/strict";
import test from "node:test";

import { controlRequestSchema, controlResponseSchema } from "../src/schemas.js";

test("outgoing control requests are exact, bounded, and action discriminated", () => {
  assert.deepEqual(controlRequestSchema.parse({ action: "boot", atMs: 12 }), {
    action: "boot",
    atMs: 12,
  });
  assert.throws(
    () => controlRequestSchema.parse({ action: "boot", atMs: 12, surprise: true }),
    /unrecognized/i,
  );
  assert.throws(
    () => controlRequestSchema.parse({ action: "noiseSample", seed: -1, occupied: true }),
  );
});

test("incoming simulator responses fail closed on unknown or missing fields", () => {
  const minimalFailure = {
    controlVersion: 1,
    kind: "result",
    accepted: false,
    duplicate: false,
    error: "invalid_control_input",
    detail: "bad input",
  };
  assert.equal(controlResponseSchema.parse(minimalFailure).accepted, false);
  assert.throws(() => controlResponseSchema.parse({ ...minimalFailure, extra: "ignored?" }));
  assert.throws(() => controlResponseSchema.parse({ ...minimalFailure, detail: 42 }));
});
