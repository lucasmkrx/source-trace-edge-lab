# Persistence and recovery model

The runtime uses one SQLite connection as its only writer. SQLite is configured
for rollback journaling, `synchronous=FULL`, foreign keys, and a bounded busy
timeout. This is a local durability model, not a distributed consensus model.

## Atomic acceptance boundary

One `BEGIN IMMEDIATE` transaction accepts a controller transition. When
applicable, the same transaction writes:

- the complete controller checkpoint;
- the next outbound sequence;
- append-only audit rows;
- newly encoded outbound frames; and
- the accepted authenticated-input sequence and signature.

If that transaction fails, none of those values advances. The live process
shows a volatile storage-fault indicator, rejects the transition, and recovery
returns the last committed state. In particular, an authenticated command is
not marked against replay until its domain effects commit.

The outbound queue is capped at 256 records and 1 MiB by default. Capacity is
checked before enqueue and checked again inside the store. Saturation is a
durable domain health state distinct from an I/O failure. Existing queued
events remain eligible for delivery, while a new session start fails closed.

## Restart sequence

At construction the runtime loads the controller checkpoint, next outbound
sequence, and authenticated-input high-water mark. Pending outbound frames and
their retry metadata remain in SQLite. The output indicator is immediately
reconstructed from the checkpoint.

On a connectivity-restored event, retry budgets are reset for the new
connection and up to 32 queued frames are visited in sequence order. Failed
sends use persisted delays of 250, 500, 1000, 2000, and 4000 milliseconds and
stop after five attempts for that connection. A later reconnect creates a new
bounded opportunity; the runtime never spins.

## Delivery semantics

Delivery is **at least once**, not exactly once. A receiver may apply an event
and the local acknowledgement delete may then fail. The frame is retransmitted
after recovery. Receivers therefore use the stable `eventId` as an idempotency
key. The simulated receiver records repeated transmissions but applies each
encoded frame only once.

## Audit integrity

Audit rows are inserted in the transition transaction. SQLite triggers reject
updates and deletes, providing append-only behavior for normal database access.
This is not tamper-proof storage: an administrator who replaces the database or
schema is outside the prototype's trust boundary.

## Deterministic control interface

`sourcetrace_sim` reads one bounded JSON object per line and writes one JSON
result per line. Supported actions are `status`, `boot`, `occupancy`, `button`,
`network`, `tick`, `storageRecovered`, `flush`, `restart`, `injectFrame`,
`faultSends`, `faultCommits`, `faultAcks`, and `noiseSample`.

The executable accepts `--db`, `--queue-records`, and `--queue-bytes`. Its demo
Ed25519 key is derived from the fixed byte seed `00..1f`; this is intentionally
public test material and must never be used as a production key.

## Known limits

- The schema is v1 and does not yet include online migrations.
- The audit table is append-only but is not cryptographically chained.
- Queue recovery is single-process; multiple writers are unsupported.
- There is no key rotation, revocation, secure element, or confidentiality.
- Disk-full behavior depends on the host filesystem and SQLite guarantees.
