from __future__ import annotations

import asyncio
import json
from pathlib import Path

from backend.feedback_service.theta import ThetaState, ThetaUpdater
from backend.federation.delta import ThetaDelta, sign_delta, verify_and_load
from backend.federation.merge import merge_deltas


def test_delta_sign_verify_roundtrip(tmp_path: Path) -> None:
    updater = ThetaUpdater(path=tmp_path / "theta.json")

    async def _capture() -> ThetaState:
        return await updater.current_state()

    state = asyncio.run(_capture())
    delta = ThetaDelta.from_state(state, noise_scale=0.0)
    signed = sign_delta(delta, b"test-key")
    restored = verify_and_load(signed, b"test-key")
    assert restored.theta[: len(state.theta)] == state.theta
    assert restored.sigma == state.sigma


def test_merge_and_persist(tmp_path: Path) -> None:
    updater = ThetaUpdater(path=tmp_path / "theta.json")

    async def _work() -> float:
        state = await updater.current_state()
        delta = ThetaDelta.from_state(state, noise_scale=0.0)
        merged = merge_deltas(state, [delta])
        await updater.persist_state(merged)
        new_state = await updater.current_state()
        return new_state.theta[0]

    value = asyncio.run(_work())
    assert value == asyncio.run(updater.current_state()).theta[0]
