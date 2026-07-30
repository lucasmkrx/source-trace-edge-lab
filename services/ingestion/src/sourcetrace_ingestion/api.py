from __future__ import annotations

import re
import uuid
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from sourcetrace_ingestion.config import Settings
from sourcetrace_ingestion.errors import FailureClass, IngestionError
from sourcetrace_ingestion.models import ErrorResponse, IngestionResponse
from sourcetrace_ingestion.processor import IngestionProcessor
from sourcetrace_ingestion.store import IdempotencyStore
from sourcetrace_ingestion.structured_logging import configure_logging

_IDENTIFIER = re.compile(r"^[A-Za-z0-9._:-]{1,128}$")


def _validated_identifier(value: str | None, *, required: bool) -> str:
    if value is None and not required:
        return str(uuid.uuid4())
    if value is None or _IDENTIFIER.fullmatch(value) is None:
        name = "X-Idempotency-Key" if required else "X-Correlation-ID"
        raise IngestionError(
            FailureClass.invalid_request,
            400,
            f"{name} must contain 1-128 safe identifier characters",
        )
    return value


def create_app(settings: Settings | None = None) -> FastAPI:
    effective_settings = settings or Settings.from_environment()
    store = IdempotencyStore(effective_settings.database_path)
    processor = IngestionProcessor(effective_settings, store, configure_logging())

    @asynccontextmanager
    async def lifespan(_app: FastAPI) -> AsyncIterator[None]:
        yield
        store.close()

    app = FastAPI(
        title="SourceTrace PDF ingestion worker",
        version="0.1.0",
        lifespan=lifespan,
    )
    app.state.processor = processor
    app.state.store = store

    @app.exception_handler(IngestionError)
    async def handle_ingestion_error(
        request: Request,
        error: IngestionError,
    ) -> JSONResponse:
        correlation_id = getattr(request.state, "correlation_id", str(uuid.uuid4()))
        response = ErrorResponse(
            correlation_id=correlation_id,
            error=error.classification,
            detail=error.detail,
        )
        return JSONResponse(
            status_code=error.status_code,
            content=response.model_dump(by_alias=True),
        )

    @app.get("/healthz")
    async def health() -> dict[str, str]:
        return {"status": "ok"}

    @app.post(
        "/v1/ingest",
        response_model=IngestionResponse,
        response_model_by_alias=True,
        responses={400: {"model": ErrorResponse}, 409: {"model": ErrorResponse}},
    )
    async def ingest(request: Request) -> IngestionResponse:
        correlation_id = _validated_identifier(
            request.headers.get("x-correlation-id"), required=False
        )
        request.state.correlation_id = correlation_id
        idempotency_key = _validated_identifier(
            request.headers.get("x-idempotency-key"), required=True
        )
        return await processor.ingest(request, idempotency_key, correlation_id)

    return app
