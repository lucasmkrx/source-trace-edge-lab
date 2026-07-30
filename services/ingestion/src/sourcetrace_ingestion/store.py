from __future__ import annotations

import sqlite3
import threading
from dataclasses import dataclass
from enum import StrEnum
from pathlib import Path
from typing import Any

from sourcetrace_ingestion.models import Artifact


class ReservationStatus(StrEnum):
    acquired = "acquired"
    cached = "cached"
    conflict = "conflict"
    in_progress = "in_progress"


@dataclass(frozen=True)
class Reservation:
    status: ReservationStatus
    artifact: Artifact | None = None


class IdempotencyStore:
    def __init__(self, path: Path) -> None:
        self._connection = sqlite3.connect(path, check_same_thread=False, isolation_level=None)
        self._lock = threading.Lock()
        with self._lock:
            self._connection.execute("PRAGMA journal_mode=DELETE")
            self._connection.execute("PRAGMA synchronous=FULL")
            self._connection.execute("PRAGMA busy_timeout=2000")
            self._connection.execute(
                """
                CREATE TABLE IF NOT EXISTS ingestion_requests(
                    idempotency_key TEXT PRIMARY KEY,
                    content_sha256 TEXT NOT NULL,
                    policy_version TEXT NOT NULL,
                    status TEXT NOT NULL CHECK(status IN ('processing','completed','failed')),
                    result_json TEXT,
                    failure_class TEXT
                )
                """
            )

    def close(self) -> None:
        with self._lock:
            self._connection.close()

    def reserve(self, key: str, content_sha256: str, policy_version: str) -> Reservation:
        with self._lock:
            self._connection.execute("BEGIN IMMEDIATE")
            try:
                row = self._connection.execute(
                    """
                    SELECT content_sha256,policy_version,status,result_json
                    FROM ingestion_requests WHERE idempotency_key=?
                    """,
                    (key,),
                ).fetchone()
                if row is None:
                    self._connection.execute(
                        """
                        INSERT INTO ingestion_requests(
                            idempotency_key,content_sha256,policy_version,status
                        ) VALUES(?,?,?,'processing')
                        """,
                        (key, content_sha256, policy_version),
                    )
                    result = Reservation(ReservationStatus.acquired)
                elif row[0] != content_sha256 or row[1] != policy_version:
                    result = Reservation(ReservationStatus.conflict)
                elif row[2] == "completed":
                    result = Reservation(
                        ReservationStatus.cached,
                        Artifact.model_validate_json(row[3]),
                    )
                elif row[2] == "processing":
                    result = Reservation(ReservationStatus.in_progress)
                else:
                    self._connection.execute(
                        """
                        UPDATE ingestion_requests
                        SET status='processing',result_json=NULL,failure_class=NULL
                        WHERE idempotency_key=?
                        """,
                        (key,),
                    )
                    result = Reservation(ReservationStatus.acquired)
                self._connection.execute("COMMIT")
                return result
            except BaseException:
                self._connection.execute("ROLLBACK")
                raise

    def complete(self, key: str, artifact: Artifact) -> None:
        with self._lock:
            cursor = self._connection.execute(
                """
                UPDATE ingestion_requests
                SET status='completed',result_json=?,failure_class=NULL
                WHERE idempotency_key=? AND status='processing'
                """,
                (artifact.model_dump_json(), key),
            )
            if cursor.rowcount != 1:
                raise RuntimeError("idempotency reservation was lost")

    def fail(self, key: str, failure_class: str) -> None:
        with self._lock:
            self._connection.execute(
                """
                UPDATE ingestion_requests
                SET status='failed',failure_class=?
                WHERE idempotency_key=? AND status='processing'
                """,
                (failure_class, key),
            )

    def raw_record(self, key: str) -> dict[str, Any] | None:
        """Test-supporting metadata inspection; document content is never stored."""
        with self._lock:
            row = self._connection.execute(
                """
                SELECT content_sha256,policy_version,status,result_json,failure_class
                FROM ingestion_requests WHERE idempotency_key=?
                """,
                (key,),
            ).fetchone()
        if row is None:
            return None
        return {
            "content_sha256": row[0],
            "policy_version": row[1],
            "status": row[2],
            "result_json": row[3],
            "failure_class": row[4],
        }
