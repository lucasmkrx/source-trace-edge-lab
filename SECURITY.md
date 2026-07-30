# Security policy

## Prototype status

SourceTrace Edge Lab is a portfolio prototype. It has not been independently
audited, certified, or deployed as a production device. Do not use it to protect
people, property, credentials, or safety-critical operations.

## Reporting a vulnerability

Until a public repository and security contact are approved, report potential
vulnerabilities privately to the repository owner. Do not include secrets,
personal data, or copyrighted documents in a report.

A future public release must replace this paragraph with an approved contact and
response expectation.

## Supported versions

Only the current local `main` branch is considered during development. No
published version is currently supported.

## Security boundaries

- CRC32 detects accidental corruption; it does not authenticate a sender.
- Ed25519 test keys are public fixtures and must never be treated as credentials.
- The simulator transport is local and provides no confidentiality.
- A privileged local user can modify the controller database and executable.
- The PDF worker is defense-in-depth demonstration code, not a complete
  hostile-document sandbox.
