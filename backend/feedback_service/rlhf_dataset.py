"""Helpers for appending feedback into an RLHF training dataset."""

from __future__ import annotations

import asyncio
import json
import os
from pathlib import Path
from typing import Optional

from .schemas import FeedbackRecord


class RLHFDatasetWriter:
    """Append feedback records into a JSONL dataset for RLHF pipelines."""

    def __init__(self, dataset_path: Optional[Path] = None) -> None:
        default_path = Path("data/rlhf_feedback.jsonl")
        if dataset_path is not None:
            path = dataset_path
        else:
            env_path = os.getenv("RLHF_DATASET_PATH")
            path = Path(env_path) if env_path else default_path

        path.parent.mkdir(parents=True, exist_ok=True)
        self._path = path

    async def append(self, record: FeedbackRecord) -> None:
        """Append the supplied feedback record to the dataset file."""

        payload = {
            "id": str(record.id),
            "conversation_id": record.conversation_id,
            "message_id": record.message_id,
            "rating": record.rating.value,
            "assistant_message": record.assistant_message,
            "user_message": record.user_message,
            "comment": record.comment,
            "mode": record.mode,
            "created_at": record.created_at.isoformat(),
        }

        await asyncio.to_thread(self._append_sync, payload)

    def _append_sync(self, payload: dict[str, object]) -> None:
        with self._path.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(payload, ensure_ascii=False) + "\n")


_dataset_writer = RLHFDatasetWriter()


async def get_dataset_writer() -> RLHFDatasetWriter:
    """FastAPI dependency returning a shared dataset writer."""

    return _dataset_writer


__all__ = ["RLHFDatasetWriter", "get_dataset_writer"]
