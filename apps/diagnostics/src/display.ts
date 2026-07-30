import type { ControlResponse, FullResult } from "./schemas.js";
import { decodeFrame } from "./protocol.js";

const rejectionExplanation: Readonly<Record<string, string>> = {
  none: "accepted",
  command_rejected: "controller domain policy rejected the requested transition",
  replay_rejected: "authenticated sequence is below the durable high-water mark",
  sequence_conflict: "authenticated sequence was reused with different content",
  stale_configuration: "configuration version did not increase",
  storage_failure: "the local atomic persistence boundary failed",
  protocol_rejected: "binary framing, integrity, authentication, or schema validation failed",
  invalid_hex: "frame input was not bounded even-length hexadecimal",
};

export function renderResponse(response: ControlResponse): string {
  if (!("state" in response)) {
    return `control error=${response.error} detail=${response.detail}`;
  }
  const lines = [
    `accepted=${String(response.accepted)} duplicate=${String(response.duplicate)} error=${
      response.error
    } (${rejectionExplanation[response.error] ?? response.detail})`,
    `state mode=${response.state.mode} health=${response.state.health} indicator=${
      response.state.indicator
    } occupied=${String(response.state.occupied)} connected=${String(
      response.state.connected,
    )}`,
    `queue records=${response.queue.records} bytes=${response.queue.bytes} receiverEffects=${
      response.receiverAppliedEffects
    }`,
  ];
  if (response.protocolErrorCode !== undefined) {
    lines.push(`protocolErrorCode=${response.protocolErrorCode}`);
  }
  appendAudits(lines, response);
  appendFrames(lines, response);
  return lines.join("\n");
}

function appendAudits(lines: string[], response: FullResult): void {
  for (const audit of response.auditTail) {
    lines.push(`audit atMs=${audit.atMs} code=${audit.code} detail=${audit.detail}`);
  }
}

function appendFrames(lines: string[], response: FullResult): void {
  for (const frameHex of response.transmittedFramesHex) {
    const frame = decodeFrame(frameHex);
    lines.push(
      `frame hex=${frameHex} type=${frame.messageType} sequence=${frame.sequence.toString()} ` +
        `payloadBytes=${frame.payloadLength} crc=${frame.crc32Hex} valid=${String(
          frame.crcValid,
        )}`,
    );
  }
}
