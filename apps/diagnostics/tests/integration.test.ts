import assert from "node:assert/strict";
import { access } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { runScenario } from "../src/scenario.js";

const diagnosticsDirectory = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const binary =
  process.env["SOURCETRACE_SIMULATOR_BINARY"] ??
  resolve(diagnosticsDirectory, "../../build/dev/edge/sourcetrace_sim");

test("diagnostic scenario drives the real C++ runtime across restart and faults", async () => {
  await access(binary);
  const summary = await runScenario({ simulatorBinary: binary });
  assert.equal(summary.steps, 15);
  assert.ok(summary.persistedQueueRecords > 0);
  assert.equal(summary.duplicateAccepted, true);
  assert.equal(summary.conflictRejected, true);
  assert.equal(summary.malformedRejected, true);
  assert.ok(summary.recoveredQueueRecords > 0);
  assert.ok(summary.receiverAppliedEffects >= summary.persistedQueueRecords);
});
