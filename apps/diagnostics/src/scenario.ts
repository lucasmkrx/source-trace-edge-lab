import assert from "node:assert/strict";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";

import { renderResponse } from "./display.js";
import {
  corruptCrc,
  demoDeviceId,
  demoPrivateKey,
  encodeFrame,
  messageType,
  toHex,
} from "./protocol.js";
import type { ControlResponse, FullResult } from "./schemas.js";
import { SimulatorClient } from "./simulator-client.js";

export interface ScenarioOptions {
  simulatorBinary: string;
  verbose?: boolean;
}

export interface ScenarioSummary {
  steps: number;
  persistedQueueRecords: number;
  duplicateAccepted: boolean;
  conflictRejected: boolean;
  malformedRejected: boolean;
  recoveredQueueRecords: number;
  receiverAppliedEffects: number;
}

function full(response: ControlResponse): FullResult {
  assert.ok("state" in response, `simulator rejected control input: ${response.detail}`);
  return response;
}

export async function runScenario(options: ScenarioOptions): Promise<ScenarioSummary> {
  const directory = await mkdtemp(join(tmpdir(), "sourcetrace-diagnostics-"));
  const client = new SimulatorClient({
    binary: options.simulatorBinary,
    database: join(directory, "runtime.sqlite3"),
  });
  let steps = 0;
  const execute = async (
    label: string,
    request: Parameters<SimulatorClient["request"]>[0],
  ): Promise<FullResult> => {
    const response = full(await client.request(request));
    steps += 1;
    if (options.verbose === true) {
      process.stdout.write(`\n[${label}]\n${renderResponse(response)}\n`);
    }
    return response;
  };

  try {
    await execute("initial status", { action: "status" });
    const booted = await execute("complete boot", { action: "boot", atMs: 1 });
    assert.equal(booted.state.mode, "idle");

    const configuration = encodeFrame(
      {
        type: messageType.configuration,
        deviceId: demoDeviceId(),
        sequence: 7n,
        monotonicMs: 10n,
        payload: {
          configVersion: 1,
          occupancyRequiredToStart: true,
          telemetryIntervalMs: 1_000,
        },
        authenticated: true,
      },
      demoPrivateKey(),
    );
    const configured = await execute("authenticated configuration", {
      action: "injectFrame",
      frameHex: toHex(configuration),
    });
    assert.equal(configured.accepted, true);
    assert.equal(configured.state.configurationVersion, 1);

    const occupied = await execute("occupancy event", {
      action: "occupancy",
      atMs: 11,
      occupied: true,
    });
    assert.equal(occupied.state.occupied, true);

    const startCommand = encodeFrame(
      {
        type: messageType.command,
        deviceId: demoDeviceId(),
        sequence: 8n,
        monotonicMs: 13n,
        payload: { action: "start_session", commandId: "cmd-start-1" },
        authenticated: true,
      },
      demoPrivateKey(),
    );
    const started = await execute("authenticated start", {
      action: "injectFrame",
      frameHex: toHex(startCommand),
    });
    assert.equal(started.state.mode, "session_active");
    const persistedQueueRecords = started.queue.records;
    assert.ok(persistedQueueRecords > 0);

    const restarted = await execute("process restart", { action: "restart" });
    assert.equal(restarted.state.mode, "session_active");
    assert.equal(restarted.queue.records, persistedQueueRecords);

    const duplicate = await execute("idempotent replay", {
      action: "injectFrame",
      frameHex: toHex(startCommand),
    });
    assert.equal(duplicate.accepted, true);
    assert.equal(duplicate.duplicate, true);
    assert.equal(duplicate.queue.records, persistedQueueRecords);

    const conflictingCommand = encodeFrame(
      {
        type: messageType.command,
        deviceId: demoDeviceId(),
        sequence: 8n,
        monotonicMs: 13n,
        payload: { action: "stop_session", commandId: "cmd-conflict-1" },
        authenticated: true,
      },
      demoPrivateKey(),
    );
    const conflict = await execute("sequence conflict", {
      action: "injectFrame",
      frameHex: toHex(conflictingCommand),
    });
    assert.equal(conflict.accepted, false);
    assert.equal(conflict.error, "sequence_conflict");

    const malformed = await execute("CRC rejection", {
      action: "injectFrame",
      frameHex: toHex(corruptCrc(configuration)),
    });
    assert.equal(malformed.accepted, false);
    assert.equal(malformed.error, "protocol_rejected");

    await execute("arm acknowledgement fault", { action: "faultAcks", count: 1 });
    const failedAcknowledgement = await execute("connect with acknowledgement failure", {
      action: "network",
      atMs: 14,
      connected: true,
    });
    assert.equal(failedAcknowledgement.accepted, false);
    assert.equal(failedAcknowledgement.error, "storage_failure");
    assert.equal(failedAcknowledgement.queue.records, persistedQueueRecords + 1);
    assert.equal(failedAcknowledgement.receiverAppliedEffects, 1);
    const deliveryBatchRecords = failedAcknowledgement.queue.records;

    const flushed = await execute("idempotent recovery flush", {
      action: "flush",
      atMs: 15,
    });
    assert.equal(flushed.accepted, true);
    assert.equal(flushed.queue.records, 0);
    assert.equal(flushed.receiverAppliedEffects, deliveryBatchRecords);

    const offline = await execute("queue while offline", {
      action: "network",
      atMs: 20,
      connected: false,
    });
    assert.ok(offline.queue.records > 0);
    const recoveredQueueRecords = offline.queue.records;

    const secondRestart = await execute("restart with offline queue", { action: "restart" });
    assert.equal(secondRestart.queue.records, recoveredQueueRecords);
    const recovered = await execute("reconnect and drain", {
      action: "network",
      atMs: 21,
      connected: true,
    });
    assert.equal(recovered.queue.records, 0);

    return {
      steps,
      persistedQueueRecords,
      duplicateAccepted: duplicate.accepted && duplicate.duplicate,
      conflictRejected: conflict.error === "sequence_conflict",
      malformedRejected: malformed.error === "protocol_rejected",
      recoveredQueueRecords,
      receiverAppliedEffects: recovered.receiverAppliedEffects,
    };
  } finally {
    await client.close();
    await rm(directory, { recursive: true, force: true });
  }
}
