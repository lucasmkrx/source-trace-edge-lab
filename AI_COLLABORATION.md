# AI collaboration policy

This repository was created with material assistance from OpenAI Codex beginning
on 2026-07-30. The human owner supplied the goals, constraints, architecture
review, scope corrections, authorization, and publication boundaries. Codex
proposed and implemented code and documentation subject to human review and
behavioral verification.

## Evidence model

Every meaningful AI-assisted commit:

1. references a dated record under `docs/ai/sessions/`;
2. describes the human specification and Codex contribution;
3. reports only commands that were actually run;
4. records important accepted, modified, and rejected suggestions;
5. lists residual risk;
6. omits a co-author trailer unless an official identity is available.

Session records summarize prompts when copying them would expose private,
copyrighted, or irrelevant material. They do not claim pre-existing code was
AI-authored.

## Review standard

Generated code is not accepted because it compiles. Trust-boundary work requires
negative tests; parsers require malformed-input tests and fuzzing; persistence
requires recovery and failure tests; documentation claims must map to observed
evidence.

## Authorship

Commits remain human-owned and locally approved. No Codex email address or
fictional reviewer identity is used.
