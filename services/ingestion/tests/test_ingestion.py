from __future__ import annotations

import asyncio
import json
import logging
import subprocess
from pathlib import Path
from typing import Any

import pytest
from fastapi.testclient import TestClient
from pypdf import PdfWriter

from sourcetrace_ingestion.api import create_app
from sourcetrace_ingestion.config import Settings
from sourcetrace_ingestion.structured_logging import JsonFormatter
from sourcetrace_ingestion.worker_helper import process

FIXTURE = Path(__file__).parent / "fixtures/original-demo.pdf"


@pytest.fixture
def pdf_bytes() -> bytes:
    return FIXTURE.read_bytes()


def settings(tmp_path: Path, **overrides: Any) -> Settings:
    values: dict[str, Any] = {
        "database_path": tmp_path / "requests.sqlite3",
        "total_deadline_seconds": 5.0,
    }
    values.update(overrides)
    return Settings(**values)


def headers(key: str = "request-1", correlation: str = "correlation-1") -> dict[str, str]:
    return {
        "Content-Type": "application/pdf",
        "X-Idempotency-Key": key,
        "X-Correlation-ID": correlation,
    }


def test_valid_pdf_is_structurally_checked_and_converted(
    tmp_path: Path, pdf_bytes: bytes, monkeypatch: pytest.MonkeyPatch
) -> None:
    app = create_app(settings(tmp_path))
    observed_directory: Path | None = None
    original = app.state.processor._run_helper

    async def capture(*args: Any, **kwargs: Any) -> Any:
        nonlocal observed_directory
        observed_directory = args[0].parent
        return await original(*args, **kwargs)

    monkeypatch.setattr(app.state.processor, "_run_helper", capture)
    with TestClient(app) as client:
        response = client.post("/v1/ingest", content=pdf_bytes, headers=headers())
    assert response.status_code == 200
    body = response.json()
    assert body["correlationId"] == "correlation-1"
    assert body["pageCount"] == 1
    assert body["textBytes"] > 0
    assert body["cached"] is False
    assert len(body["contentSha256"]) == 64
    assert len(body["textSha256"]) == 64
    assert observed_directory is not None
    assert not observed_directory.exists()


def test_duplicate_request_returns_cached_metadata(tmp_path: Path, pdf_bytes: bytes) -> None:
    app = create_app(settings(tmp_path))
    with TestClient(app) as client:
        first = client.post("/v1/ingest", content=pdf_bytes, headers=headers())
        second = client.post(
            "/v1/ingest",
            content=pdf_bytes,
            headers=headers(correlation="correlation-2"),
        )
    assert first.status_code == 200
    assert second.status_code == 200
    assert second.json()["cached"] is True
    assert second.json()["contentSha256"] == first.json()["contentSha256"]
    assert second.json()["correlationId"] == "correlation-2"


def test_conflicting_idempotency_key_is_rejected(tmp_path: Path, pdf_bytes: bytes) -> None:
    app = create_app(settings(tmp_path))
    with TestClient(app) as client:
        assert client.post("/v1/ingest", content=pdf_bytes, headers=headers()).status_code == 200
        changed = pdf_bytes + b"\n"
        response = client.post("/v1/ingest", content=changed, headers=headers())
    assert response.status_code == 409
    assert response.json()["error"] == "idempotency_conflict"


@pytest.mark.parametrize(
    ("request_headers", "body", "expected_status", "expected_error"),
    [
        (
            {"Content-Type": "text/plain", "X-Idempotency-Key": "key"},
            b"%PDF-no",
            415,
            "unsupported_media_type",
        ),
        (
            {"Content-Type": "application/pdf", "X-Idempotency-Key": "key"},
            b"not a pdf",
            422,
            "invalid_pdf",
        ),
        (
            {"Content-Type": "application/pdf"},
            b"%PDF-no",
            400,
            "invalid_request",
        ),
    ],
)
def test_declared_type_magic_and_required_key_are_validated(
    tmp_path: Path,
    request_headers: dict[str, str],
    body: bytes,
    expected_status: int,
    expected_error: str,
) -> None:
    app = create_app(settings(tmp_path))
    with TestClient(app) as client:
        response = client.post("/v1/ingest", content=body, headers=request_headers)
    assert response.status_code == expected_status
    assert response.json()["error"] == expected_error


def test_declared_oversize_is_rejected_before_streaming(tmp_path: Path, pdf_bytes: bytes) -> None:
    app = create_app(settings(tmp_path, max_input_bytes=len(pdf_bytes) - 1))
    with TestClient(app) as client:
        response = client.post("/v1/ingest", content=pdf_bytes, headers=headers())
    assert response.status_code == 413
    assert response.json()["error"] == "input_too_large"


def test_actual_stream_size_is_enforced_despite_smaller_declaration(
    tmp_path: Path, pdf_bytes: bytes
) -> None:
    maximum = len(pdf_bytes) - 1
    request_headers = headers()
    request_headers["Content-Length"] = str(maximum)
    app = create_app(settings(tmp_path, max_input_bytes=maximum))
    with TestClient(app) as client:
        response = client.post("/v1/ingest", content=pdf_bytes, headers=request_headers)
    assert response.status_code == 413
    assert response.json()["detail"].startswith("streamed")


def test_structurally_invalid_pdf_has_specific_failure(tmp_path: Path) -> None:
    app = create_app(settings(tmp_path))
    with TestClient(app) as client:
        response = client.post(
            "/v1/ingest",
            content=b"%PDF-1.7\nnot structurally valid",
            headers=headers(),
        )
    assert response.status_code == 422
    assert response.json()["error"] == "invalid_pdf"


def test_page_limit_is_checked_before_conversion(tmp_path: Path) -> None:
    path = tmp_path / "two-pages.pdf"
    writer = PdfWriter()
    writer.add_blank_page(width=72, height=72)
    writer.add_blank_page(width=72, height=72)
    with path.open("wb") as output:
        writer.write(output)
    app = create_app(settings(tmp_path, max_pages=1))
    with TestClient(app) as client:
        response = client.post("/v1/ingest", content=path.read_bytes(), headers=headers())
    assert response.status_code == 422
    assert response.json()["error"] == "page_limit_exceeded"


def test_missing_converter_is_classified_and_temporary_files_are_cleaned(
    tmp_path: Path, pdf_bytes: bytes, monkeypatch: pytest.MonkeyPatch
) -> None:
    app = create_app(settings(tmp_path, pdftotext_binary="missing-pdftotext-binary"))
    observed_directory: Path | None = None
    original = app.state.processor._run_helper

    async def capture(*args: Any, **kwargs: Any) -> Any:
        nonlocal observed_directory
        observed_directory = args[0].parent
        return await original(*args, **kwargs)

    monkeypatch.setattr(app.state.processor, "_run_helper", capture)
    with TestClient(app) as client:
        response = client.post("/v1/ingest", content=pdf_bytes, headers=headers())
    assert response.status_code == 422
    assert response.json()["error"] == "conversion_failed"
    assert observed_directory is not None
    assert not observed_directory.exists()


def test_total_deadline_marks_reservation_failed_and_cleans_up(
    tmp_path: Path, pdf_bytes: bytes, monkeypatch: pytest.MonkeyPatch
) -> None:
    app = create_app(settings(tmp_path, total_deadline_seconds=0.01))
    observed_directory: Path | None = None

    async def slow_helper(input_path: Path, *_args: Any) -> Any:
        nonlocal observed_directory
        observed_directory = input_path.parent
        await asyncio.sleep(1)
        raise AssertionError("deadline did not cancel helper")

    monkeypatch.setattr(app.state.processor, "_run_helper", slow_helper)
    with TestClient(app) as client:
        response = client.post("/v1/ingest", content=pdf_bytes, headers=headers("deadline-key"))
        record = app.state.store.raw_record("deadline-key")
    assert response.status_code == 504
    assert response.json()["error"] == "deadline_exceeded"
    assert record is not None
    assert record["status"] == "failed"
    assert record["failure_class"] == "deadline_exceeded"
    assert observed_directory is not None
    assert not observed_directory.exists()


def test_helper_uses_argument_array_and_shell_false(
    tmp_path: Path, pdf_bytes: bytes, monkeypatch: pytest.MonkeyPatch
) -> None:
    input_path = tmp_path / "input.pdf"
    output_path = tmp_path / "output.txt"
    input_path.write_bytes(pdf_bytes)
    observed: dict[str, Any] = {}

    def fake_run(arguments: list[str], **kwargs: Any) -> subprocess.CompletedProcess[bytes]:
        observed["arguments"] = arguments
        observed.update(kwargs)
        output_path.write_text("synthetic extracted text", encoding="utf-8")
        return subprocess.CompletedProcess(arguments, 0)

    monkeypatch.setattr(subprocess, "run", fake_run)
    metadata = process(input_path, output_path, 2, "pdftotext", 1.0)
    assert metadata["pageCount"] == 1
    assert isinstance(observed["arguments"], list)
    assert observed["arguments"][0] == "pdftotext"
    assert observed["shell"] is False


def test_structured_formatter_redacts_unapproved_fields() -> None:
    formatter = JsonFormatter()
    record = logging.LogRecord("test", logging.INFO, "", 0, "ignored", (), None)
    record.structured = {
        "event": "ingestion_completed",
        "correlation_id": "safe-correlation",
        "idempotency_key": "secret-key",
        "content_sha256": "secret-hash",
        "input_path": "/secret/path",
    }
    rendered = formatter.format(record)
    parsed = json.loads(rendered)
    assert parsed["event"] == "ingestion_completed"
    assert parsed["correlation_id"] == "safe-correlation"
    assert "secret" not in rendered
