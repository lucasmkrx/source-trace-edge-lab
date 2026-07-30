# Security policy

## Prototype status

SourceTrace Edge Lab is a portfolio prototype. It has not been independently
audited, certified, or deployed as a production device. Do not use it to protect
people, property, credentials, or safety-critical operations.

## Reporting a vulnerability

Use the public repository's **Report a vulnerability** flow to send the
maintainer a private security advisory. Do not open a public issue for an
unfixed vulnerability, and do not include real secrets, personal data, or
copyrighted documents in a report.

The maintainer will acknowledge a report within seven calendar days. This is a
portfolio project without a production security service-level agreement.

## Supported versions

Only the current `main` branch is considered for this prototype. No release is
supported for production use.

## Security boundaries

- CRC32 detects accidental corruption; it does not authenticate a sender.
- Ed25519 test keys are public fixtures and must never be treated as credentials.
- The simulator transport is local and provides no confidentiality.
- A privileged local user can modify the controller database and executable.
- The PDF worker is defense-in-depth demonstration code, not a complete
  hostile-document sandbox.
- The static public site serves documentation only and exposes no controller,
  ingestion, database, or administration endpoint.
