# PDF ingestion security boundary

The ingestion service accepts a raw `application/pdf` request body at
`POST /v1/ingest`. It returns metadata only; document bytes and extracted text
are not retained after processing.

## Acceptance sequence

1. Validate a bounded idempotency key and correlation identifier.
2. Require the declared media type `application/pdf`.
3. Reject an oversized `Content-Length` before reading the body.
4. Stream into a private temporary directory while hashing and enforcing the
   actual five MiB default limit.
5. Require the `%PDF-` magic prefix.
6. Reserve `(idempotency key, content SHA-256, policy version)` in SQLite.
7. Start a separate Python helper with an argument array and `shell=False`.
8. Apply address-space, output-file, CPU, and open-file limits where the host
   kernel supports them.
9. Parse with strict `pypdf`, reject encrypted/empty/malformed documents, and
   enforce the 32-page default before conversion.
10. Invoke `pdftotext` with a fixed argument array, `shell=False`, no stdin, a
    bounded timeout, and captured output.
11. Verify helper metadata and extracted-file size, persist only hashes and
    counts, and remove the temporary directory.

An `asyncio.timeout` surrounds upload, hashing, reservation, helper startup,
parsing, and conversion. Cancellation kills and reaps the helper. The helper
receives an allow-listed environment so test instrumentation, credentials, and
unrelated process state are not inherited.

## Idempotency

A completed request with the same key, content hash, and `pdf-v1` policy returns
cached metadata. The same key with different content or policy returns HTTP 409.
A concurrent equivalent request returns HTTP 409 while its reservation is
processing. Classified failures may be retried with the same key and content.

SQLite stores the idempotency key, content hash, policy version, status,
classification, and result metadata. It does not store the document or
extracted text.

## Logging and errors

Logs are JSON and use an allow list: event, correlation ID, failure class,
duration, page count, and cache status. Idempotency keys, content hashes,
temporary paths, document bytes, extracted text, and helper stderr are omitted.

Public classifications distinguish invalid requests, media type, size, PDF
structure, page count, conversion, deadline, idempotency conflict, in-progress
work, and internal failure. Responses do not expose exception strings.

## Container boundary

The image is based on a digest-pinned official Python 3.12.13 Alpine image,
installs Poppler from signed Alpine repositories, and runs as UID 10001. The
container smoke test uses a read-only root filesystem, drops all capabilities,
and mounts bounded 16 MiB writable `tmpfs` paths for `/tmp` and `/data`. This
also demonstrates that no persistent document artifact is required.

## Known limits

- PDF parsing is high-risk native/data-processing work despite isolation and
  bounds; the prototype is not an antivirus or content-disarm system.
- Resource-limit availability and enforcement vary by kernel.
- A process crash can leave a `processing` reservation; leases/reaping are
  deferred.
- SQLite is local single-instance idempotency, not a distributed lock.
- Poppler and pypdf require ongoing vulnerability and license review.
- No OCR, Office conversion, cloud storage, or production deployment is in the
  first release.
