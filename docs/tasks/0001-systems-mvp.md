# Task 0001: systems portfolio MVP

Date: 2026-07-30

## Human specification

Create a clean, local, C++-centered portfolio artifact demonstrating a
deterministic classroom edge controller, bounded authenticated byte-stream
protocol, offline persistence and recovery, a hardened local Python PDF worker,
a strict TypeScript diagnostic client, behavioral verification, and truthful AI
provenance. Do not copy private product content into the artifact, create a
remote, deploy, or publish.

After Commit 5 the human amended placement: preserve the lab's focused history
under `source-trace-edge-lab/` on a new branch of the original private
`tatiana` repository and delete the standalone checkout. This superseded the
earlier “original repository unchanged” instruction. The lab subtree remains
the only portfolio boundary; the parent is not a publication candidate.

## Acceptance criteria

The authoritative criteria are recorded in the approved implementation request
and summarized in the seven focused session records. Every claimed command must
have an observed result.

## Principal risks

- Parser allocation before length validation.
- Confusing CRC corruption detection with authentication.
- Replay state not committed with command effects.
- Claiming exactly-once delivery despite possible retransmission.
- Unbounded storage, retry, logging, or document processing.
- UI and documentation work displacing executable systems evidence.
- Accidental inclusion of private or copyrighted source material.
