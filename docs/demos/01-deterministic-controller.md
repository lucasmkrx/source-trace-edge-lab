# Demo 1: deterministic safe-state transitions

Interview time: 5–7 minutes.

## Claim

The controller's policy is deterministic and independently testable. Network
loss degrades remote visibility without stopping a local active session;
storage failure blocks a new start but cannot block a physical stop.

## Run

```sh
cmake --preset dev
cmake --build --preset dev
build/dev/edge/sourcetrace_core_tests \
  "storage failure blocks a new session but never blocks a physical stop"
build/dev/edge/sourcetrace_core_tests \
  "replaying the same trace is byte-for-behavior deterministic"
```

Then open `edge/src/core/reducer.cpp` and
`edge/simulator/deterministic_simulator.cpp`.

## Narrate

- Time enters as an event value; the reducer never reads a clock.
- Mode and health are separate, so an active local session can be network-
  degraded without being semantically stopped.
- The reducer requests effects but cannot perform storage/network/hardware I/O.
- Same-timestamp simulator inputs retain deterministic insertion order.

## Expected evidence

Both focused Catch2 cases pass. The full CTest suite is the stronger regression
gate; focused filters are only for interview pacing.

Do not claim formal verification, real-time scheduling, or hardware validation.
