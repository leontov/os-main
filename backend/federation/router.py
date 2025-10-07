from __future__ import annotations

import os
from typing import List

from fastapi import APIRouter, Depends, HTTPException

from ..feedback_service.theta import ThetaUpdater, get_theta_updater
from .delta import ThetaDelta, sign_delta, verify_and_load
from .merge import merge_deltas

router = APIRouter(prefix="/api/federation", tags=["federation"])

FEDERATION_KEY = os.getenv("KOLIBRI_FEDERATION_KEY", "kolibri-federation").encode("utf-8")


@router.post("/export")
async def export_delta(
    updater: ThetaUpdater = Depends(get_theta_updater),
) -> dict[str, str]:
    state = await updater.current_state()
    delta = ThetaDelta.from_state(state, noise_scale=float(os.getenv("KOLIBRI_DP_NOISE", "0.0")))
    signed = sign_delta(delta, FEDERATION_KEY)
    return {"delta": signed}


@router.post("/merge")
async def merge_delta(
    payload: dict,
    updater: ThetaUpdater = Depends(get_theta_updater),
) -> dict[str, str]:
    raw = payload.get("delta")
    if not isinstance(raw, str):
        raise HTTPException(status_code=400, detail="delta missing")
    delta = verify_and_load(raw, FEDERATION_KEY)
    state = await updater.current_state()
    merged = merge_deltas(state, [delta])
    await updater.persist_state(merged)
    return {"status": "ok"}
