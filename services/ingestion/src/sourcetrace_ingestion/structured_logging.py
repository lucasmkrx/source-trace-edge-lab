from __future__ import annotations

import json
import logging
from collections.abc import Mapping
from typing import Any


class JsonFormatter(logging.Formatter):
    """Render allow-listed structured fields without document or key material."""

    _allowed = frozenset(
        {
            "event",
            "correlation_id",
            "failure_class",
            "duration_ms",
            "page_count",
            "cached",
        }
    )

    def format(self, record: logging.LogRecord) -> str:
        payload: dict[str, Any] = {"level": record.levelname.lower()}
        fields = getattr(record, "structured", {})
        if isinstance(fields, Mapping):
            payload.update({key: value for key, value in fields.items() if key in self._allowed})
        return json.dumps(payload, separators=(",", ":"), sort_keys=True)


def configure_logging() -> logging.Logger:
    logger = logging.getLogger("sourcetrace.ingestion")
    if not logger.handlers:
        handler = logging.StreamHandler()
        handler.setFormatter(JsonFormatter())
        logger.addHandler(handler)
    logger.setLevel(logging.INFO)
    logger.propagate = True
    return logger
