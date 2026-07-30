from __future__ import annotations

import asyncio
import hashlib
import json
import logging
import math
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any

from fastapi import Request

from sourcetrace_ingestion.config import Settings
from sourcetrace_ingestion.errors import FailureClass, IngestionError
from sourcetrace_ingestion.models import Artifact, IngestionResponse
from sourcetrace_ingestion.store import IdempotencyStore, ReservationStatus


class IngestionProcessor:
    def __init__(
        self,
        settings: Settings,
        store: IdempotencyStore,
        logger: logging.Logger,
    ) -> None:
        self.settings = settings
        self.store = store
        self._logger = logger

    async def ingest(
        self,
        request: Request,
        idempotency_key: str,
        correlation_id: str,
    ) -> IngestionResponse:
        started = time.monotonic()
        reserved = False
        try:
            async with asyncio.timeout(self.settings.total_deadline_seconds):
                self._validate_declared_input(request)
                with tempfile.TemporaryDirectory(prefix="sourcetrace-ingestion-") as directory:
                    input_path = Path(directory) / "input.pdf"
                    output_path = Path(directory) / "extracted.txt"
                    content_sha256 = await self._stream_input(request, input_path)
                    reservation = self.store.reserve(
                        idempotency_key,
                        content_sha256,
                        self.settings.policy_version,
                    )
                    if reservation.status is ReservationStatus.conflict:
                        raise IngestionError(
                            FailureClass.idempotency_conflict,
                            409,
                            "idempotency key was already used for different content or policy",
                        )
                    if reservation.status is ReservationStatus.in_progress:
                        raise IngestionError(
                            FailureClass.processing_in_progress,
                            409,
                            "an equivalent request is already processing",
                        )
                    if reservation.status is ReservationStatus.cached:
                        if reservation.artifact is None:
                            raise RuntimeError("completed reservation has no result")
                        response = self._response(
                            reservation.artifact, correlation_id, cached=True
                        )
                        self._log_success(started, correlation_id, response)
                        return response

                    reserved = True
                    artifact = await self._run_helper(input_path, output_path, content_sha256)
                    self.store.complete(idempotency_key, artifact)
                    reserved = False
                    response = self._response(artifact, correlation_id, cached=False)
                    self._log_success(started, correlation_id, response)
                    return response
        except TimeoutError as error:
            failure = IngestionError(
                FailureClass.deadline_exceeded,
                504,
                "total PDF processing deadline exceeded",
            )
            if reserved:
                self.store.fail(idempotency_key, failure.classification)
            self._log_failure(started, correlation_id, failure.classification)
            raise failure from error
        except IngestionError as error:
            if reserved:
                self.store.fail(idempotency_key, error.classification)
            self._log_failure(started, correlation_id, error.classification)
            raise
        except Exception as error:
            if reserved:
                self.store.fail(idempotency_key, FailureClass.internal_failure)
            self._log_failure(started, correlation_id, FailureClass.internal_failure)
            raise IngestionError(
                FailureClass.internal_failure,
                500,
                "internal PDF processing failure",
            ) from error

    def _validate_declared_input(self, request: Request) -> None:
        media_type = request.headers.get("content-type", "").split(";", maxsplit=1)[0].lower()
        if media_type != "application/pdf":
            raise IngestionError(
                FailureClass.unsupported_media_type,
                415,
                "Content-Type must be application/pdf",
            )
        declared_length = request.headers.get("content-length")
        if declared_length is None:
            return
        try:
            length = int(declared_length)
        except ValueError as error:
            raise IngestionError(
                FailureClass.invalid_request,
                400,
                "Content-Length must be a non-negative integer",
            ) from error
        if length < 0:
            raise IngestionError(
                FailureClass.invalid_request,
                400,
                "Content-Length must be a non-negative integer",
            )
        if length > self.settings.max_input_bytes:
            raise IngestionError(
                FailureClass.input_too_large,
                413,
                "declared PDF size exceeds the configured limit",
            )

    async def _stream_input(self, request: Request, destination: Path) -> str:
        digest = hashlib.sha256()
        total = 0
        magic = bytearray()
        with destination.open("xb") as output:
            async for chunk in request.stream():
                if not chunk:
                    continue
                total += len(chunk)
                if total > self.settings.max_input_bytes:
                    raise IngestionError(
                        FailureClass.input_too_large,
                        413,
                        "streamed PDF size exceeds the configured limit",
                    )
                if len(magic) < 5:
                    magic.extend(chunk[: 5 - len(magic)])
                digest.update(chunk)
                output.write(chunk)
        if bytes(magic) != b"%PDF-":
            raise IngestionError(
                FailureClass.invalid_pdf,
                422,
                "input does not begin with PDF magic",
            )
        return digest.hexdigest()

    async def _run_helper(
        self,
        input_path: Path,
        output_path: Path,
        content_sha256: str,
    ) -> Artifact:
        cpu_seconds = max(1, math.ceil(self.settings.total_deadline_seconds))
        arguments = [
            sys.executable,
            "-m",
            "sourcetrace_ingestion.worker_helper",
            "--input",
            str(input_path),
            "--output",
            str(output_path),
            "--max-pages",
            str(self.settings.max_pages),
            "--memory-bytes",
            str(self.settings.helper_memory_bytes),
            "--output-bytes",
            str(self.settings.helper_output_bytes),
            "--cpu-seconds",
            str(cpu_seconds),
            "--conversion-timeout-seconds",
            str(self.settings.total_deadline_seconds),
            "--pdftotext",
            self.settings.pdftotext_binary,
        ]
        helper_environment = {
            key: os.environ[key]
            for key in ("LANG", "LC_ALL", "PATH", "SYSTEMROOT", "TMPDIR")
            if key in os.environ
        }
        helper_environment["PYTHONNOUSERSITE"] = "1"
        process = subprocess.Popen(  # noqa: S603 - fixed Python executable and argument array
            arguments,
            shell=False,
            env=helper_environment,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        try:
            stdout, _stderr = await asyncio.to_thread(
                process.communicate,
                timeout=self.settings.total_deadline_seconds,
            )
        except subprocess.TimeoutExpired as error:
            process.kill()
            await asyncio.to_thread(process.wait)
            raise IngestionError(
                FailureClass.deadline_exceeded,
                504,
                "PDF helper process deadline exceeded",
            ) from error
        except asyncio.CancelledError:
            process.kill()
            await asyncio.to_thread(process.wait)
            raise

        return_code = process.returncode
        if return_code != 0:
            classifications = {
                10: (
                    FailureClass.invalid_pdf,
                    422,
                    "PDF structure validation failed",
                ),
                11: (
                    FailureClass.page_limit_exceeded,
                    422,
                    "PDF page count exceeds the configured limit",
                ),
                12: (
                    FailureClass.conversion_failed,
                    422,
                    "pdftotext conversion failed",
                ),
            }
            classification, status, detail = classifications.get(
                return_code,
                (
                    FailureClass.conversion_failed,
                    422,
                    "PDF helper process failed",
                ),
            )
            raise IngestionError(classification, status, detail)

        try:
            metadata: dict[str, Any] = json.loads(stdout)
            page_count = int(metadata["pageCount"])
            text_bytes = int(metadata["textBytes"])
        except (KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
            raise RuntimeError("helper emitted invalid metadata") from error
        if page_count < 1 or page_count > self.settings.max_pages:
            raise RuntimeError("helper emitted out-of-policy page count")
        extracted = output_path.read_bytes()
        if len(extracted) != text_bytes or text_bytes > self.settings.helper_output_bytes:
            raise RuntimeError("helper output size does not match metadata")
        return Artifact(
            content_sha256=content_sha256,
            policy_version=self.settings.policy_version,
            page_count=page_count,
            text_sha256=hashlib.sha256(extracted).hexdigest(),
            text_bytes=text_bytes,
        )

    @staticmethod
    def _response(
        artifact: Artifact,
        correlation_id: str,
        *,
        cached: bool,
    ) -> IngestionResponse:
        return IngestionResponse(
            correlation_id=correlation_id,
            content_sha256=artifact.content_sha256,
            policy_version=artifact.policy_version,
            page_count=artifact.page_count,
            text_sha256=artifact.text_sha256,
            text_bytes=artifact.text_bytes,
            cached=cached,
        )

    def _log_success(
        self,
        started: float,
        correlation_id: str,
        response: IngestionResponse,
    ) -> None:
        self._logger.info(
            "ingestion completed",
            extra={
                "structured": {
                    "event": "ingestion_completed",
                    "correlation_id": correlation_id,
                    "duration_ms": round((time.monotonic() - started) * 1000),
                    "page_count": response.page_count,
                    "cached": response.cached,
                }
            },
        )

    def _log_failure(
        self,
        started: float,
        correlation_id: str,
        classification: FailureClass,
    ) -> None:
        self._logger.warning(
            "ingestion failed",
            extra={
                "structured": {
                    "event": "ingestion_failed",
                    "correlation_id": correlation_id,
                    "duration_ms": round((time.monotonic() - started) * 1000),
                    "failure_class": classification,
                }
            },
        )
