from __future__ import annotations

from enum import StrEnum


class FailureClass(StrEnum):
    invalid_request = "invalid_request"
    unsupported_media_type = "unsupported_media_type"
    input_too_large = "input_too_large"
    invalid_pdf = "invalid_pdf"
    page_limit_exceeded = "page_limit_exceeded"
    conversion_failed = "conversion_failed"
    deadline_exceeded = "deadline_exceeded"
    idempotency_conflict = "idempotency_conflict"
    processing_in_progress = "processing_in_progress"
    internal_failure = "internal_failure"


class IngestionError(Exception):
    def __init__(self, classification: FailureClass, status_code: int, detail: str) -> None:
        super().__init__(detail)
        self.classification = classification
        self.status_code = status_code
        self.detail = detail
