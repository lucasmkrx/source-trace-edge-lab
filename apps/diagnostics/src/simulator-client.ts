import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { once } from "node:events";
import { createInterface } from "node:readline";

import {
  controlRequestSchema,
  controlResponseSchema,
  type ControlRequest,
  type ControlResponse,
} from "./schemas.js";

interface Pending {
  resolve: (value: ControlResponse) => void;
  reject: (error: Error) => void;
  timer: NodeJS.Timeout;
}

export interface SimulatorOptions {
  binary: string;
  database?: string;
  queueRecords?: number;
  queueBytes?: number;
  responseTimeoutMs?: number;
}

export class SimulatorClient {
  readonly #child: ChildProcessWithoutNullStreams;
  readonly #pending: Pending[] = [];
  readonly #responseTimeoutMs: number;
  #stderr = "";
  #closed = false;

  constructor(options: SimulatorOptions) {
    const arguments_: string[] = [];
    if (options.database !== undefined) {
      arguments_.push("--db", options.database);
    }
    if (options.queueRecords !== undefined) {
      arguments_.push("--queue-records", String(options.queueRecords));
    }
    if (options.queueBytes !== undefined) {
      arguments_.push("--queue-bytes", String(options.queueBytes));
    }
    this.#responseTimeoutMs = options.responseTimeoutMs ?? 5_000;
    this.#child = spawn(options.binary, arguments_, {
      shell: false,
      stdio: ["pipe", "pipe", "pipe"],
    });
    this.#child.stderr.setEncoding("utf8");
    this.#child.stderr.on("data", (chunk: string) => {
      this.#stderr = (this.#stderr + chunk).slice(-4_096);
    });
    createInterface({ input: this.#child.stdout, crlfDelay: Number.POSITIVE_INFINITY }).on(
      "line",
      (line) => this.#receive(line),
    );
    this.#child.once("error", (error) => this.#rejectAll(error));
    this.#child.once("exit", (code, signal) => {
      this.#closed = true;
      this.#rejectAll(
        new Error(
          `simulator exited before response (code=${String(code)}, signal=${String(signal)}): ${
            this.#stderr
          }`,
        ),
      );
    });
  }

  request(input: ControlRequest): Promise<ControlResponse> {
    if (this.#closed) {
      return Promise.reject(new Error("simulator is closed"));
    }
    const validated = controlRequestSchema.parse(input);
    return new Promise<ControlResponse>((resolve, reject) => {
      const timer = setTimeout(() => {
        const index = this.#pending.findIndex((candidate) => candidate.timer === timer);
        if (index >= 0) {
          this.#pending.splice(index, 1);
        }
        reject(new Error(`simulator response timed out: ${this.#stderr}`));
      }, this.#responseTimeoutMs);
      this.#pending.push({ resolve, reject, timer });
      this.#child.stdin.write(`${JSON.stringify(validated)}\n`, "utf8", (error) => {
        if (error !== null && error !== undefined) {
          clearTimeout(timer);
          const index = this.#pending.findIndex((candidate) => candidate.timer === timer);
          if (index >= 0) {
            this.#pending.splice(index, 1);
          }
          reject(error);
        }
      });
    });
  }

  async close(): Promise<void> {
    if (this.#closed) {
      return;
    }
    this.#child.stdin.end();
    const timeout = setTimeout(() => this.#child.kill("SIGKILL"), 2_000);
    await once(this.#child, "exit");
    clearTimeout(timeout);
  }

  #receive(line: string): void {
    const pending = this.#pending.shift();
    if (pending === undefined) {
      this.#child.kill("SIGKILL");
      return;
    }
    clearTimeout(pending.timer);
    try {
      const value: unknown = JSON.parse(line);
      pending.resolve(controlResponseSchema.parse(value));
    } catch (error) {
      pending.reject(error instanceof Error ? error : new Error(String(error)));
      this.#child.kill("SIGKILL");
    }
  }

  #rejectAll(error: Error): void {
    for (const pending of this.#pending.splice(0)) {
      clearTimeout(pending.timer);
      pending.reject(error);
    }
  }
}
