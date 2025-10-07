from __future__ import annotations

import asyncio
import importlib
import os
from typing import Any, AsyncIterator, Optional, Protocol
from urllib.parse import urlparse
from uuid import uuid4

from .schemas import FeedbackPayload, FeedbackRecord


class FeedbackStorageError(RuntimeError):
    """Raised when feedback persistence fails."""


class FeedbackStorage(Protocol):
    """Protocol describing the feedback persistence interface."""

    async def save_feedback(self, payload: FeedbackPayload) -> FeedbackRecord:
        """Persist the feedback payload and return the stored record."""

        ...

    async def close(self) -> None:
        """Release all resources allocated by the storage implementation."""

        ...


def _load_asyncpg() -> Any:
    """Load the asyncpg module lazily to avoid import-time failures."""

    return importlib.import_module("asyncpg")


def _load_clickhouse_connect() -> Any:
    """Load clickhouse-connect lazily to avoid import-time failures."""

    return importlib.import_module("clickhouse_connect")


class PostgresFeedbackStorage:
    """Persist feedback in a PostgreSQL database using asyncpg."""

    def __init__(self, dsn: str) -> None:
        self._dsn = dsn
        self._pool: Any = None
        self._lock = asyncio.Lock()

    async def _ensure_pool(self) -> Any:
        if self._pool is None:
            async with self._lock:
                if self._pool is None:
                    asyncpg = _load_asyncpg()
                    try:
                        self._pool = await asyncpg.create_pool(self._dsn)
                    except Exception as exc:  # pragma: no cover - network errors
                        raise FeedbackStorageError("Не удалось подключиться к PostgreSQL.") from exc
        return self._pool

    async def save_feedback(self, payload: FeedbackPayload) -> FeedbackRecord:
        pool = await self._ensure_pool()
        record_id = uuid4()
        record = FeedbackRecord.create(record_id=record_id, payload=payload)

        try:
            async with pool.acquire() as connection:
                await connection.execute(
                    """
                    INSERT INTO feedback (
                        id,
                        conversation_id,
                        message_id,
                        rating,
                        assistant_message,
                        user_message,
                        comment,
                        mode,
                        created_at
                    )
                    VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
                    """,
                    record.id,
                    record.conversation_id,
                    record.message_id,
                    record.rating.value,
                    record.assistant_message,
                    record.user_message,
                    record.comment,
                    record.mode,
                    record.created_at,
                )
        except Exception as exc:  # pragma: no cover - network errors
            raise FeedbackStorageError("Не удалось сохранить отзыв в PostgreSQL.") from exc

        return record

    async def close(self) -> None:
        if self._pool is not None:
            await self._pool.close()
            self._pool = None


class ClickHouseFeedbackStorage:
    """Persist feedback in ClickHouse using clickhouse-connect."""

    def __init__(self, dsn: str) -> None:
        self._dsn = dsn
        self._client: Any = None
        self._lock = asyncio.Lock()

    def _client_kwargs(self) -> dict[str, object]:
        parsed = urlparse(self._dsn)
        database = parsed.path.lstrip("/") or "default"
        port = parsed.port
        if port is None:
            port = 8443 if parsed.scheme.endswith("s") else 8123

        return {
            "host": parsed.hostname or "localhost",
            "port": port,
            "username": parsed.username or "default",
            "password": parsed.password or "",
            "database": database,
            "secure": parsed.scheme.endswith("s"),
        }

    async def _ensure_client(self) -> Any:
        if self._client is None:
            async with self._lock:
                if self._client is None:
                    clickhouse_connect = _load_clickhouse_connect()
                    try:
                        self._client = await asyncio.to_thread(
                            clickhouse_connect.get_client, **self._client_kwargs()
                        )
                    except Exception as exc:  # pragma: no cover - network errors
                        raise FeedbackStorageError("Не удалось подключиться к ClickHouse.") from exc
        return self._client

    async def save_feedback(self, payload: FeedbackPayload) -> FeedbackRecord:
        client = await self._ensure_client()
        record_id = uuid4()
        record = FeedbackRecord.create(record_id=record_id, payload=payload)

        data = [
            [
                str(record.id),
                record.conversation_id,
                record.message_id,
                record.rating.value,
                record.assistant_message,
                record.user_message,
                record.comment,
                record.mode,
                record.created_at,
            ]
        ]
        columns = [
            "id",
            "conversation_id",
            "message_id",
            "rating",
            "assistant_message",
            "user_message",
            "comment",
            "mode",
            "created_at",
        ]

        try:
            await asyncio.to_thread(client.insert, "feedback", data, column_names=columns)
        except Exception as exc:  # pragma: no cover - network errors
            raise FeedbackStorageError("Не удалось сохранить отзыв в ClickHouse.") from exc

        return record

    async def close(self) -> None:
        if self._client is not None:
            await asyncio.to_thread(self._client.close)
            self._client = None


_storage_instance: Optional[FeedbackStorage] = None
_storage_lock = asyncio.Lock()


def _create_storage_from_env() -> FeedbackStorage:
    dsn = os.getenv("FEEDBACK_DATABASE_URL")
    if not dsn:
        raise FeedbackStorageError(
            "Не задана переменная окружения FEEDBACK_DATABASE_URL для подключения к базе."
        )

    scheme = urlparse(dsn).scheme.lower()

    if scheme.startswith("postgres"):
        return PostgresFeedbackStorage(dsn)

    if scheme.startswith("clickhouse"):
        return ClickHouseFeedbackStorage(dsn)

    raise FeedbackStorageError(
        "Поддерживаются только подключения PostgreSQL (postgres://) и ClickHouse (clickhouse://)."
    )


async def get_feedback_storage() -> AsyncIterator[FeedbackStorage]:
    """FastAPI dependency that returns a cached storage instance."""

    global _storage_instance

    if _storage_instance is None:
        async with _storage_lock:
            if _storage_instance is None:
                _storage_instance = _create_storage_from_env()

    assert _storage_instance is not None
    yield _storage_instance


async def shutdown_feedback_storage() -> None:
    """Dispose the cached storage instance during application shutdown."""

    global _storage_instance
    if _storage_instance is not None:
        await _storage_instance.close()
        _storage_instance = None


__all__ = [
    "ClickHouseFeedbackStorage",
    "FeedbackStorage",
    "FeedbackStorageError",
    "PostgresFeedbackStorage",
    "get_feedback_storage",
    "shutdown_feedback_storage",
]
