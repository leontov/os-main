"""Kolibri feedback service package."""

from __future__ import annotations

from typing import Any

__all__ = ["app"]


def __getattr__(name: str) -> Any:  # pragma: no cover - ленивый импорт для совместимости
    if name == "app":
        from .main import app as fastapi_app

        return fastapi_app
    raise AttributeError(name)
