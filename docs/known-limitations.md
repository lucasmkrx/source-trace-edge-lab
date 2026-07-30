# Known limitations

- This is a local portfolio prototype with no independent audit,
  certification, production deployment, field trial, or hardware-in-the-loop
  validation.
- Protocol v1 has no confidentiality, enrollment, key rotation, revocation,
  secure element, secure boot, attestation, authorization roles, or certificate
  chain. Deterministic keys are public fixtures.
- Monotonic milliseconds order diagnostic events but are not a remote freshness
  proof. The runtime assumes one authenticated sender/device high-water mark.
- SQLite supports one process/writer. Durability depends on the host filesystem,
  storage hardware, OS, and SQLite. Append-only triggers are not tamper-proof.
- The queue is deliberately bounded. Saturation rejects new session work rather
  than silently dropping existing events; capacity planning and operator
  recovery are not implemented.
- Delivery is at least once. Every real receiver would need durable
  idempotency. The simulator proves its own receiver behavior only.
- Retry timing is deterministic policy, not a production network stack. There
  is no TLS, discovery, session negotiation, congestion control, or broker.
- The simulator uses JSON Lines as a local control interface. That interface is
  not the binary device protocol and is not hardened as a remote API.
- The PDF worker is defense in depth, not antivirus, content disarm, or a
  formally isolated sandbox. Host resource-limit support varies. Processing
  leases, OCR, distributed idempotency, malware scanning, and read-only root
  filesystem enforcement are deferred.
- Dependency inventories and vulnerability results are snapshots. A live npm
  audit was not authorized in the final local run; the cache-only audit found
  no known production vulnerability. Python versions are resolved exactly but
  the requirements file does not yet include artifact hashes.
- CI files inside this directory become active only if the subtree is extracted
  as a repository root. While nested in `tatiana`, the parent workflow owns CI.
- The parent repository contains product-specific and private material and must
  not be treated as part of the portfolio publication set.
