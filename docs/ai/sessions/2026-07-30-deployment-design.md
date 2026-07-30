# AI session: deployment boundary

- Date: 2026-07-30
- Goal: publish the reviewer experience through an isolated Google Cloud
  project and custom hostname without expanding the prototype's attack surface.
- Human direction: create a new Google Cloud project, deploy at
  `demo.marquesco.com`, and preserve an exemplary AI co-working history.
- AI tool: OpenAI Codex.

## Decision sequence

1. Codex created the isolated project `source-trace-edge-lab-demo`.
2. Cloud Run was considered for container delivery, but linking the project to
   the approved billing account returned `Cloud billing quota exceeded`.
3. Codex treated that failure as a design constraint, not a reason to weaken
   billing controls or reuse an unrelated project.
4. The public artifact was narrowed to Firebase Hosting in the same isolated
   Google Cloud project. This matches the actual static workload and removes a
   public application runtime.

## Security choices

- Deploy only `showcase/public/`.
- Keep the simulator, PDF worker, databases, test fixtures, and credentials out
  of the hosted artifact.
- Use no external scripts, fonts, analytics, forms, or network calls.
- Apply a default-deny content security policy and restrictive browser headers.
- Use the exact DNS records produced by Firebase for the custom domain.
- Keep deployment and rollback commands reviewable in a runbook.

## Verification before release

- Hosting configuration is contract-tested with Node's built-in test runner.
- The publication-scope scan and full secret/history scan are release gates.
- The deployed response, headers, custom domain, certificate, and browser
  interactions will be recorded only after they have been observed live.

## Residual risk

Firebase and DNS control-plane compromise remain upstream risks. Static hosting
does not make the repository's prototype implementation production-ready. The
site explains verified local evidence; it does not execute the controller.
