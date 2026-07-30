import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { createInterface } from "node:readline/promises";
import { fileURLToPath } from "node:url";

import { renderResponse } from "./display.js";
import {
  corruptCrc,
  demoDeviceId,
  demoPrivateKey,
  encodeFrame,
  messageType,
  toHex,
} from "./protocol.js";
import { runScenario } from "./scenario.js";
import type { ControlRequest } from "./schemas.js";
import { SimulatorClient } from "./simulator-client.js";

const diagnosticsDirectory = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const labRoot = resolve(diagnosticsDirectory, "../..");
const simulatorBinary =
  process.env["SOURCETRACE_SIMULATOR_BINARY"] ??
  join(labRoot, "build", "dev", "edge", "sourcetrace_sim");

async function main(): Promise<void> {
  const command = process.argv[2] ?? "interactive";
  if (command === "scenario") {
    const summary = await runScenario({ simulatorBinary, verbose: true });
    process.stdout.write(`\nscenario passed ${JSON.stringify(summary)}\n`);
    return;
  }
  if (command !== "interactive") {
    throw new Error("usage: npm start -- [interactive|scenario]");
  }
  await interactive();
}

async function interactive(): Promise<void> {
  const directory = await mkdtemp(join(tmpdir(), "sourcetrace-console-"));
  const client = new SimulatorClient({
    binary: simulatorBinary,
    database: join(directory, "runtime.sqlite3"),
  });
  const input = createInterface({ input: process.stdin, output: process.stdout });
  let nowMs = 0;
  let sequence = 1n;
  const key = demoPrivateKey();
  process.stdout.write(
    "SourceTrace diagnostics. Commands: status, boot, occupancy on|off, button, " +
      "network on|off, restart, config, command start|stop|status, malformed, quit\n",
  );
  try {
    while (true) {
      const line = (await input.question("diagnostics> ")).trim();
      if (line === "quit" || line === "exit") {
        break;
      }
      const request = parseInteractive(line, ++nowMs, sequence, key);
      if (request === undefined) {
        process.stdout.write("unknown or incomplete command\n");
        continue;
      }
      if (request.action === "injectFrame" && line !== "malformed") {
        sequence += 1n;
      }
      process.stdout.write(`${renderResponse(await client.request(request))}\n`);
    }
  } finally {
    input.close();
    await client.close();
    await rm(directory, { recursive: true, force: true });
  }
}

function parseInteractive(
  line: string,
  atMs: number,
  sequence: bigint,
  key: ReturnType<typeof demoPrivateKey>,
): ControlRequest | undefined {
  const [command, argument] = line.split(/\s+/, 2);
  if (command === "status") {
    return { action: "status" };
  }
  if (command === "boot") {
    return { action: "boot", atMs };
  }
  if (command === "button") {
    return { action: "button", atMs };
  }
  if (command === "restart") {
    return { action: "restart" };
  }
  if (command === "occupancy" && (argument === "on" || argument === "off")) {
    return { action: "occupancy", atMs, occupied: argument === "on" };
  }
  if (command === "network" && (argument === "on" || argument === "off")) {
    return { action: "network", atMs, connected: argument === "on" };
  }
  if (command === "config") {
    return {
      action: "injectFrame",
      frameHex: toHex(
        encodeFrame(
          {
            type: messageType.configuration,
            deviceId: demoDeviceId(),
            sequence,
            monotonicMs: BigInt(atMs),
            payload: {
              configVersion: Number(sequence),
              occupancyRequiredToStart: true,
              telemetryIntervalMs: 1_000,
            },
            authenticated: true,
          },
          key,
        ),
      ),
    };
  }
  if (
    command === "command" &&
    (argument === "start" || argument === "stop" || argument === "status")
  ) {
    const action =
      argument === "start"
        ? "start_session"
        : argument === "stop"
          ? "stop_session"
          : "request_status";
    return {
      action: "injectFrame",
      frameHex: toHex(
        encodeFrame(
          {
            type: messageType.command,
            deviceId: demoDeviceId(),
            sequence,
            monotonicMs: BigInt(atMs),
            payload: { action, commandId: `console-${sequence.toString()}` },
            authenticated: true,
          },
          key,
        ),
      ),
    };
  }
  if (command === "malformed") {
    const frame = encodeFrame({
      type: messageType.telemetry,
      deviceId: demoDeviceId(),
      sequence,
      monotonicMs: BigInt(atMs),
      payload: {
        eventId: "console-malformed",
        health: "nominal",
        mode: "idle",
        occupied: false,
        queueDepth: 0,
      },
    });
    return { action: "injectFrame", frameHex: toHex(corruptCrc(frame)) };
  }
  return undefined;
}

await main();
