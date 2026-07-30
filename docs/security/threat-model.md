# Threat model

## Scope and assets

Protected prototype assets are controller integrity, authenticated command
effects, durable replay state, outbound event ordering, bounded process memory,
audit records under normal database access, document confidentiality during
local processing, and accurate diagnostic output.

The model covers malformed/untrusted protocol bytes, untrusted PDF bytes,
temporary network loss, process restart, duplicate delivery, and injected local
storage/transport failures. It does not cover an administrator who can replace
the executable, SQLite database, schema, test key, kernel, or container runtime.

## Trust boundaries

| Boundary | Untrusted input | Validation/containment |
|---|---|---|
| Binary device stream | Lengths, enums, flags, JSON, signature, sequence | Fixed buffer; ordered bounds/header/CRC/signature/schema checks |
| Authenticated runtime | Validly signed but duplicate/stale/conflicting input | Durable high-water mark and signature comparison committed with effects |
| SQLite | I/O failure, full queue, failed acknowledgement | Transaction rollback, explicit capacity, persisted retry state, fault indication |
| Transport receiver | Retransmission after lost local acknowledgement | Stable `eventId` receiver deduplication |
| PDF HTTP request | Headers, streamed bytes, PDF structure | Size/type/magic/schema limits, deadline, private temp directory |
| PDF tool process | Parser/converter crash, stall, excessive resources | Separate helper, fixed argv, `shell=False`, limits, kill/reap, cleanup |
| Diagnostic child process | Malformed, missing, unsolicited, or stalled JSON | Exact Zod response, FIFO request, timeout, bounded stderr, terminate on violation |

## Threats and controls

- **Memory/resource exhaustion:** outer frame length is rejected after four
  bytes; payload and JSON shapes are bounded; one feed returns at most 32
  results. PDF input, pages, output, time, address space, CPU, files, and open
  descriptors are bounded where the host supports each limit.
- **Command forgery:** configuration and command messages require an Ed25519
  signature over a domain-separated header and raw payload. CRC32 is checked
  earlier only to detect corruption.
- **Replay and equivocation:** an identical high-water sequence/signature is an
  idempotent duplicate, lower sequences are replay, and the same sequence with
  a different signature is conflict. Acceptance state is durable with effects.
- **Partial persistence:** one SQLite transaction contains the controller
  checkpoint, audit rows, outbound queue additions, next sequence, and replay
  record. Tests inject commit and acknowledgement failures.
- **Lost acknowledgement:** retransmission can occur; the receiver deduplicates
  complete frame identity/event ID. No exactly-once claim is made.
- **Shell/path injection:** child tools and simulator use argument arrays with
  `shell=False`; temporary paths are process-created rather than user chosen.
- **Sensitive document retention/logging:** only hashes, counts, policy,
  status, and classifications persist. Logs use an allow list and temporary
  document/text files are removed.
- **Supply-chain drift:** core versions are pinned in CMake commits, an exact
  Python resolved-version file, npm lockfiles, container base digest, and
  GitHub action commit SHAs in the lab workflow. The Python file does not
  contain artifact hashes; this remains a publication follow-up.

## Residual threats

Ed25519 fixture keys have no enrollment, rotation, revocation, secure element,
or compromise recovery. The local stream has no confidentiality. SQLite audit
triggers do not stop privileged replacement. PDF libraries remain a complex
native/data attack surface and the helper is defense in depth, not a proven
sandbox. Dependency vulnerability data becomes stale between scans.

The parent `tatiana` repository is outside this threat model and outside the
lab's publication boundary.
