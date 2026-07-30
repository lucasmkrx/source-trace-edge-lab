# Local publication checklist

No public release is authorized. This checklist describes a future human-
approved process and makes the current blocker explicit.

## Current gate

- [x] Lab source, tests, fixture, documentation, license, and AI provenance are
  contained under `source-trace-edge-lab/`.
- [x] The lab has focused implementation history preserved on the local
  `codex/source-trace-edge-lab` branch.
- [ ] The parent `tatiana` repository is safe to publish. **Blocked by design:**
  it contains product-specific code, migration assets, cloud identifiers, and
  private material outside the lab boundary.
- [ ] A clean extracted candidate repository exists. Not created or authorized.

Never publish the current parent repository as the portfolio artifact.

## Future extraction gate

- [ ] Obtain explicit authorization to create a separate candidate repository.
- [ ] Export only `source-trace-edge-lab/`; do not copy the parent `.git`,
  sibling files, remotes, branches, tags, or configuration.
- [ ] Decide whether to preserve the five original focused lab commits by
  filtered history or create a reviewed clean release history.
- [ ] Confirm the author name/email are appropriate for public history.
- [ ] Verify `git rev-list --objects --all` contains only intended lab paths.

## Content and security gate

- [ ] Inspect every tracked binary. Confirm the sole PDF fixture regenerates
  byte for byte from `services/ingestion/tools/generate_fixture.py`.
- [ ] Search all candidate files and history for secrets, credentials, internal
  domains, personal data, product branding, cloud resource identifiers,
  copyrighted educational content, and temporary paths.
- [ ] Run a current secret scanner against the complete candidate history and
  record its version/output.
- [ ] Review `.env.example`, GitHub workflows, container metadata, issue/PR
  templates, commit bodies, authors, and tags.
- [ ] Confirm deterministic demo keys are clearly marked public and no real key
  material exists.

## Dependency and license gate

- [ ] Rebuild from lockfiles in a clean environment.
- [ ] Regenerate the Python lock with artifact hashes, then run current
  vulnerability audits for CMake dependencies, Python, npm, container OS
  packages, and GitHub Actions.
- [ ] Generate an SBOM and compare it to `THIRD_PARTY_LICENSES.md`.
- [ ] Confirm Apache-2.0 `LICENSE`, `NOTICE`, third-party notices, and fixture
  authorship are correct with qualified legal review if required.

## Verification and release gate

- [ ] Pass Linux and macOS C++ builds, all tests, ASan/UBSan, clang-tidy, and a
  seeded fuzz smoke from the extracted candidate.
- [ ] Pass Python Ruff, strict mypy, tests, coverage threshold, container scan,
  and final-byte container smoke.
- [ ] Pass TypeScript typecheck, build, audit, golden-vector tests, and the real
  C++ integration scenario.
- [ ] Have a second human review security claims, threat model, FMEA, known
  limits, AI records, and all three demos.
- [ ] Replace the placeholder private security-reporting instruction with an
  approved public contact and response policy.
- [ ] Create the remote, visibility, release, and any publication only after
  separate explicit authorization.
