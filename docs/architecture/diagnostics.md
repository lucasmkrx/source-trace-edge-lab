# Diagnostic console architecture

The diagnostics application is a strict TypeScript client for the real
`sourcetrace_sim` executable. It does not reimplement controller decisions.
The child process owns controller state, SQLite transactions, replay state,
fault injection, and transport behavior.

## Boundary

The console spawns the simulator with an argument array and `shell: false`.
Each request is validated by an exact Zod schema, serialized as one JSON object
per line, and matched FIFO with one bounded response line. Responses are parsed
as unknown JSON and must satisfy the complete exact response schema before any
field is displayed. A malformed response, unexpected line, process failure, or
five-second timeout rejects the request and terminates the child.

JSON Lines is a local control plane only. Controller messages still use the v1
binary frame described in `docs/protocol/v1.md`. The diagnostics application
contains an independent TypeScript frame encoder/decoder so a reviewer can
inspect complete frame hex, header values, payload length, and CRC. The same
golden vector is asserted by C++ and TypeScript tests.

## Deterministic scenario

`npm run scenario` creates a private temporary SQLite database, drives the
compiled C++ process through 15 steps, checks every invariant, and removes the
database:

1. boot, accept a signed configuration, observe occupancy, and accept a signed
   start command while offline;
2. reconstruct the active session and outbound queue after process restart;
3. accept an identical authenticated replay as a duplicate but reject reused
   sequence content as a conflict;
4. reject a frame with a corrupted CRC before application handling;
5. inject a local acknowledgement failure after receiver application, then
   retransmit without duplicating the receiver effect;
6. queue a later connectivity event offline, restart again, reconnect, and
   drain the queue.

The demo device identifier and Ed25519 seed are public deterministic fixtures.
They are interoperability material, not credentials.

## Run

From the repository root:

```sh
cmake --preset dev
cmake --build --preset dev
cd apps/diagnostics
npm ci
npm test
npm run scenario
```

Set `SOURCETRACE_SIMULATOR_BINARY` to test a different compiled simulator.
Run `npm start` for an interactive terminal. The console supports status, boot,
occupancy, physical button, connectivity, restart, signed configuration,
signed command, and malformed-frame actions.

## Known limits

- The console is intentionally terminal-only; a GUI would add surface area
  without strengthening the systems evidence.
- Request/response matching assumes the simulator preserves line order.
- The five-second response deadline is a diagnostic policy, not a production
  service-level objective.
- Full frame hex is appropriate for synthetic fixtures but could be sensitive
  in a real deployment.
