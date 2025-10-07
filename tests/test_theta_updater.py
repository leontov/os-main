"""Проверка инкрементального апдейтера θ по пользовательской обратной связи."""

from __future__ import annotations

import asyncio
from dataclasses import dataclass
from pathlib import Path
import json
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from backend.feedback_service.theta import ThetaUpdater


def _run(coro):
    return asyncio.run(coro)


@dataclass
class StubRecord:
    rating: str
    assistant_message: str
    user_message: str | None = None
    comment: str | None = None
    mode: str | None = None


def _make_record(rating: str, assistant_message: str = "Ответ Колибри") -> StubRecord:
    return StubRecord(
        rating=rating,
        assistant_message=assistant_message,
        user_message="Что такое Колибри?",
        comment=None,
        mode="Быстрый ответ",
    )


def test_theta_updater_accumulates_feedback(tmp_path: Path) -> None:
    state_path = tmp_path / "theta.json"
    updater = ThetaUpdater(path=state_path, learning_rate=0.4)

    positive = _make_record("useful")
    _run(updater.update(positive))
    theta_after_positive = _run(updater.current_theta())

    assert theta_after_positive, "вектор θ не должен быть пустым"
    assert theta_after_positive[0] > 1.0
    assert state_path.exists()
    assert state_path.with_suffix(".csv").exists()
    bin_path = state_path.with_suffix(".bin")
    assert bin_path.exists()
    assert bin_path.stat().st_size <= 768

    negative = _make_record("not_useful", assistant_message="Ответ без контекста")
    _run(updater.update(negative))
    theta_after_negative = _run(updater.current_theta())

    assert theta_after_negative[0] < theta_after_positive[0]

    reloaded = ThetaUpdater(path=state_path)
    persisted_theta = _run(reloaded.current_theta())
    assert persisted_theta == pytest.approx(theta_after_negative)
    payload = json.loads(state_path.read_text(encoding="utf-8"))
    assert len(payload.get("pi", [])) >= 1
    assert len(payload.get("rho", [])) >= 1
