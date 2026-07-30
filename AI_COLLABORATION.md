# AI collaboration policy

This lab was created with material assistance from OpenAI Codex beginning on
2026-07-30. It began as a clean local sibling repository; the human later
directed that its full focused history be imported under
`tatiana/source-trace-edge-lab` on a new branch and that the standalone checkout
be deleted. The human owner supplied the goals, constraints, architecture
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

The enclosing private repository is not claimed as AI-authored or safe to
publish. Provenance and publication claims in this directory apply only to the
lab subtree.
