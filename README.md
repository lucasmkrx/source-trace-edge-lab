# SourceTrace Edge Lab

SourceTrace Edge Lab is a local portfolio prototype for a deterministic
classroom edge terminal. Its centerpiece is a portable C++20 controller that
models occupancy, a physical session-control button, a status indicator,
temporary network loss, authenticated commands, bounded persistence, and
restart recovery.

The repository is being built as executable evidence of systems engineering.
It is not a production device, safety system, election system, or certified
security product.

## Current status

Implementation is in progress. The final reviewer guide and observed
verification results will be added only after the corresponding code has run.

## Repository boundaries

- `edge/`: C++20 domain, protocol, persistence, simulator, CLI, tests, fuzzing.
- `services/ingestion/`: bounded local PDF worker.
- `apps/diagnostics/`: strict TypeScript terminal diagnostic client.
- `docs/`: architecture, protocol, security, recovery, demos, and AI evidence.

No cloud credentials or production infrastructure are required.

## License

Apache License 2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
