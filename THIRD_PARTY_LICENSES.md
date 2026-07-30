# Third-party software

This inventory is updated whenever a dependency is introduced. Exact resolved
versions and transitive dependencies are recorded by lockfiles and the generated
SBOM.

| Dependency | Purpose | License |
|---|---|---|
| Catch2 | C++ behavioral tests | Boost Software License 1.0 |
| JSON for Modern C++ | Bounded JSON payload parsing | MIT |
| libsodium | Ed25519 detached-signature verification | ISC |
| SQLite | Local transactional persistence | Public domain |
| FastAPI | Local Python HTTP service | MIT |
| pydantic | Python runtime validation | MIT |
| pypdf | PDF structural validation | BSD-3-Clause |
| uvicorn | Local ASGI server | BSD-3-Clause |
| pytest | Python behavioral tests | MIT |
| Zod | TypeScript runtime validation | MIT |
| TypeScript | Type checking and compilation | Apache-2.0 |

Poppler's `pdftotext` executable is installed in the worker container under the
licenses supplied by the Debian package. It is invoked as a separate process and
is not redistributed in source form by this repository.

This file is an engineering inventory, not legal advice. The publication
checklist requires comparing it to the final lockfiles and container packages.
