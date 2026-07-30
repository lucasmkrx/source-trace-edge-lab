import { z } from "zod";

const safeInteger = z.number().int().nonnegative().safe();
const positiveCount = z.number().int().positive().safe();

export const controlRequestSchema = z.discriminatedUnion("action", [
  z.object({ action: z.literal("status") }).strict(),
  z.object({ action: z.literal("boot"), atMs: safeInteger }).strict(),
  z
    .object({ action: z.literal("occupancy"), atMs: safeInteger, occupied: z.boolean() })
    .strict(),
  z.object({ action: z.literal("button"), atMs: safeInteger }).strict(),
  z
    .object({ action: z.literal("network"), atMs: safeInteger, connected: z.boolean() })
    .strict(),
  z.object({ action: z.literal("tick"), atMs: safeInteger }).strict(),
  z.object({ action: z.literal("storageRecovered"), atMs: safeInteger }).strict(),
  z.object({ action: z.literal("flush"), atMs: safeInteger }).strict(),
  z.object({ action: z.literal("faultSends"), count: positiveCount }).strict(),
  z.object({ action: z.literal("faultCommits"), count: positiveCount }).strict(),
  z.object({ action: z.literal("faultAcks"), count: positiveCount }).strict(),
  z.object({ action: z.literal("restart") }).strict(),
  z
    .object({
      action: z.literal("injectFrame"),
      frameHex: z.string().regex(/^(?:[0-9a-f]{2})+$/i).max(8424),
    })
    .strict(),
  z
    .object({
      action: z.literal("noiseSample"),
      seed: safeInteger.max(0xffff_ffff),
      occupied: z.boolean(),
      noisePercent: safeInteger.max(100),
    })
    .strict(),
]);

const stateSchema = z
  .object({
    mode: z.enum(["booting", "idle", "session_active"]),
    health: z.enum(["nominal", "network_degraded", "queue_saturated", "storage_fault"]),
    indicator: z.enum(["booting", "ready", "active", "degraded", "fault"]),
    occupied: z.boolean(),
    connected: z.boolean(),
    configurationValid: z.boolean(),
    configurationVersion: safeInteger,
    monotonicMs: safeInteger,
    lastTelemetryMs: safeInteger,
    transitionCount: safeInteger,
  })
  .strict();

const auditSchema = z
  .object({
    atMs: safeInteger,
    code: z.string().min(1).max(64),
    detail: z.string().max(256),
  })
  .strict();

export const fullResultSchema = z
  .object({
    controlVersion: z.literal(1),
    kind: z.literal("result"),
    accepted: z.boolean(),
    duplicate: z.boolean(),
    error: z.string().min(1).max(64),
    detail: z.string().max(512),
    protocolErrorCode: safeInteger.max(0xffff).optional(),
    sample: z.boolean().optional(),
    state: stateSchema,
    queue: z
      .object({
        records: safeInteger,
        bytes: safeInteger,
      })
      .strict(),
    receiverAppliedEffects: safeInteger,
    transmittedFramesHex: z.array(z.string().regex(/^(?:[0-9a-f]{2})+$/i).max(8424)).max(32),
    auditTail: z.array(auditSchema).max(16),
  })
  .strict();

const inputFailureSchema = z
  .object({
    controlVersion: z.literal(1),
    kind: z.literal("result"),
    accepted: z.literal(false),
    duplicate: z.literal(false),
    error: z.literal("invalid_control_input"),
    detail: z.string().max(512),
  })
  .strict();

export const controlResponseSchema = z.union([fullResultSchema, inputFailureSchema]);

export type ControlRequest = z.infer<typeof controlRequestSchema>;
export type ControlResponse = z.infer<typeof controlResponseSchema>;
export type FullResult = z.infer<typeof fullResultSchema>;
