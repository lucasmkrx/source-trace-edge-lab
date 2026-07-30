# AI session: public CI correction

- Date: 2026-07-30
- Trigger: the first GitHub Actions run against the extracted public repository.
- Run: <https://github.com/lucasmkrx/source-trace-edge-lab/actions/runs/30590031638>
- AI tool: OpenAI Codex.

## Observed failures

- The TypeScript and showcase jobs failed during setup because their pinned
  `actions/setup-node` commit did not exist upstream.
- The Linux developer and sanitizer configurations found SQLite but failed
  generation because the runner's `FindSQLite3` module exported
  `SQLite::SQLite3` while the project linked `SQLite3::SQLite3`.
- macOS C++ and Python jobs passed, which kept the correction scoped to action
  runtime pins and the portable imported-target name.

## Correction

- Resolve official action major branches, pin their current full commit hashes,
  and use their Node 24-based majors.
- Select either documented SQLite imported-target spelling after discovery,
  preferring the modern name when the installed CMake provides it.
- Preserve the failed run as visible evidence instead of rewriting the
  publication history.

## Verification contract

The correction is complete only when:

1. local CMake configure, build, and all 33 C++ tests pass;
2. local TypeScript, Python, showcase, and hosting checks still pass; and
3. a new GitHub Actions run passes every job on the public `main` branch.
