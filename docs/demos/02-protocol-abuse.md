# Demo 2: bounded protocol abuse and authentication

Interview time: 7–10 minutes.

## Claim

The byte-stream parser rejects attacker-controlled lengths before body
allocation, distinguishes corruption from authentication, and separates
cryptographic acceptance from durable replay decisions.

## Run

```sh
cmake --preset dev
cmake --build --preset dev
build/dev/edge/sourcetrace_protocol_tests \
  "oversized outer length is rejected after four bytes without body allocation"
build/dev/edge/sourcetrace_protocol_tests \
  "signature corruption with a valid CRC is rejected cryptographically"

cd apps/diagnostics
npm ci
npm test
```

Open `edge/src/protocol/parser.cpp`,
`edge/src/protocol/payload_validation.cpp`, and
`apps/diagnostics/tests/protocol.test.ts`.

## Narrate

- The four-byte prefix must describe 48–4208 body bytes before the fixed buffer
  accepts more input.
- CRC32 is checked before expensive signature/JSON work, but it authenticates
  nothing.
- Configuration and command signatures cover a domain string, fixed header,
  and raw payload.
- Replay high-water state advances in the runtime transaction, not in a parser.
- C++ and TypeScript reproduce one normative frame byte for byte.

## Expected evidence

The focused C++ cases pass, then all seven Node tests pass including the shared
golden vector and real-process integration.

Do not claim confidentiality, key lifecycle, or that fuzzing proves absence of
parser vulnerabilities.
