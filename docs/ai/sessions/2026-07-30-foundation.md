# AI session: clean portfolio foundation

- Date: 2026-07-30
- Goal: initialize a clean local repository with publication controls, build
  foundations, CI structure, and truthful AI provenance.
- Human-authored specification: approved Task 0001 and the required first commit.
- AI tool: OpenAI Codex.

## Proposed approach

Create only repository foundations: Apache-2.0 licensing, notices, security
limits, publication inventory, CMake/Python/TypeScript skeletons, AI records, and
least-privilege CI. Copy no private product code.

## Risks identified

- Accidental inheritance of Git objects or private identifiers.
- Dependency versions that do not resolve.
- Documentation implying tests already passed.
- Commit metadata exposing an unintended email address.

## Suggestions

- Accepted: new repository, three-language skeleton, explicit prototype limits,
  and verification matrix using “pending” rather than inferred passes.
- Modified: the initial AI recommendation used a license placeholder; the human
  explicitly approved Apache-2.0, so the complete license is included.
- Rejected by human: the earlier broad platform-first release. See
  `docs/ai/rejected-suggestions.md`.

## Files affected

Repository metadata, root build configuration, language foundations, CI
workflow, task record, and AI documentation.

## Verification

Observed on macOS:

- Initial `cmake --preset dev && cmake --build --preset dev`: failed because
  CMake was not installed. CMake 4.4.1 and Ninja 1.13.2 were installed, and the
  rerun configured and built successfully.
- Initial TypeScript install used the shell's Node 21.3.0 and reported an engine
  mismatch. Node 22.23.2 was installed and selected explicitly.
- `npm install --ignore-scripts`: 11 packages audited, zero vulnerabilities.
- `npm run typecheck`: passed with no diagnostics.
- `npm test`: one behavioral test passed.
- `./tools/verify-foundation.sh`: passed, including the private-project marker
  scan.
- `git count-objects -vH` before the first commit: zero loose and packed
  objects, confirming no inherited Git objects.
- `git diff --check`: passed.
- The first non-escalated build attempt could not write inside the sibling
  repository because of the workspace sandbox. The same commands succeeded
  after using the explicitly authorized filesystem permission.

## Human review and final rationale

The human approved the implementation plan and authorized local commits. Final
publication remains prohibited until separately approved.

## Residual risk

Dependency and action licenses require comparison to resolved lockfiles. The
configured commit-author email must be reviewed before publication.
