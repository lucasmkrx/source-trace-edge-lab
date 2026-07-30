from __future__ import annotations

from pydantic import BaseModel, ConfigDict, Field


def _camel_case(value: str) -> str:
    first, *rest = value.split("_")
    return first + "".join(part.capitalize() for part in rest)


class Artifact(BaseModel):
    model_config = ConfigDict(extra="forbid")

    content_sha256: str = Field(pattern=r"^[0-9a-f]{64}$")
    policy_version: str
    page_count: int = Field(ge=1)
    text_sha256: str = Field(pattern=r"^[0-9a-f]{64}$")
    text_bytes: int = Field(ge=0)


class IngestionResponse(BaseModel):
    model_config = ConfigDict(alias_generator=_camel_case, populate_by_name=True)

    correlation_id: str
    content_sha256: str
    policy_version: str
    page_count: int
    text_sha256: str
    text_bytes: int
    cached: bool


class ErrorResponse(BaseModel):
    model_config = ConfigDict(alias_generator=_camel_case, populate_by_name=True)

    correlation_id: str
    error: str
    detail: str
