"""Data models for the Kolibri feedback API."""

from __future__ import annotations

from dataclasses import dataclass
import sys
from datetime import datetime, timezone
from enum import Enum
from typing import Literal, Optional
from uuid import UUID

from pydantic import BaseModel, Field, field_validator


class FeedbackRating(str, Enum):
    """Possible rating values supplied by the UI."""

    USEFUL = "useful"
    NOT_USEFUL = "not_useful"


class FeedbackPayload(BaseModel):
    """Incoming payload describing feedback for a single assistant message."""

    conversation_id: str = Field(..., min_length=1, max_length=128)
    message_id: str = Field(..., min_length=1, max_length=128)
    rating: FeedbackRating
    assistant_message: str = Field(..., min_length=1)
    user_message: Optional[str] = Field(default=None)
    comment: Optional[str] = Field(default=None, max_length=1000)
    mode: Optional[str] = Field(default=None, max_length=128)

    @field_validator("user_message")
    @classmethod
    def _strip_optional(cls, value: Optional[str]) -> Optional[str]:
        if value is None:
            return None
        stripped = value.strip()
        return stripped or None

    @field_validator("comment")
    @classmethod
    def _normalise_comment(cls, value: Optional[str]) -> Optional[str]:
        if value is None:
            return None
        stripped = value.strip()
        return stripped or None


class FeedbackResponse(BaseModel):
    """Response returned by the API after successful persistence."""

    status: Literal["ok"] = "ok"


# Python 3.10+ поддерживает slots в dataclass; на более старших версиях используем обычный декоратор.
if sys.version_info >= (3, 10):
    dataclass_slots = lambda cls: dataclass(cls, slots=True)  # type: ignore[misc]
else:  # pragma: no cover - ветка для совместимости с Python < 3.10
    dataclass_slots = dataclass


@dataclass_slots
class FeedbackRecord:
    """Canonical representation of stored feedback used across the pipeline."""

    id: UUID
    conversation_id: str
    message_id: str
    rating: FeedbackRating
    assistant_message: str
    user_message: Optional[str]
    comment: Optional[str]
    mode: Optional[str]
    created_at: datetime

    @classmethod
    def create(
        cls,
        *,
        record_id: UUID,
        payload: FeedbackPayload,
        created_at: Optional[datetime] = None,
    ) -> "FeedbackRecord":
        """Build a record instance from the incoming payload."""

        return cls(
            id=record_id,
            conversation_id=payload.conversation_id,
            message_id=payload.message_id,
            rating=payload.rating,
            assistant_message=payload.assistant_message,
            user_message=payload.user_message,
            comment=payload.comment,
            mode=payload.mode,
            created_at=created_at or datetime.now(timezone.utc),
        )


__all__ = [
    "FeedbackPayload",
    "FeedbackRating",
    "FeedbackRecord",
    "FeedbackResponse",
]
