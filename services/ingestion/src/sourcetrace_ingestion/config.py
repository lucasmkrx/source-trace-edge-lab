from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Settings:
    database_path: Path = Path("ingestion.sqlite3")
    max_input_bytes: int = 5 * 1024 * 1024
    max_pages: int = 32
    total_deadline_seconds: float = 15.0
    helper_memory_bytes: int = 256 * 1024 * 1024
    helper_output_bytes: int = 8 * 1024 * 1024
    policy_version: str = "pdf-v1"
    pdftotext_binary: str = "pdftotext"

    @classmethod
    def from_environment(cls) -> Settings:
        return cls(
            database_path=Path(os.getenv("SOURCETRACE_INGESTION_DB", "ingestion.sqlite3")),
            max_input_bytes=int(os.getenv("SOURCETRACE_MAX_INPUT_BYTES", str(5 * 1024 * 1024))),
            max_pages=int(os.getenv("SOURCETRACE_MAX_PAGES", "32")),
            total_deadline_seconds=float(os.getenv("SOURCETRACE_DEADLINE_SECONDS", "15")),
            helper_memory_bytes=int(
                os.getenv("SOURCETRACE_HELPER_MEMORY_BYTES", str(256 * 1024 * 1024))
            ),
            helper_output_bytes=int(
                os.getenv("SOURCETRACE_HELPER_OUTPUT_BYTES", str(8 * 1024 * 1024))
            ),
            policy_version=os.getenv("SOURCETRACE_POLICY_VERSION", "pdf-v1"),
            pdftotext_binary=os.getenv("SOURCETRACE_PDFTOTEXT", "pdftotext"),
        )
