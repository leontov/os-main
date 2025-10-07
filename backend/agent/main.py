from __future__ import annotations

import hashlib
from typing import Optional

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, conint

from backend.federation.router import router as federation_router

from .engine import KolibriAgent

app = FastAPI(title="Kolibri Agent API", version="0.5.0")
app.include_router(federation_router)

_agent: Optional[KolibriAgent] = None


class StepRequest(BaseModel):
    q: str | int
    beam: conint(ge=1, le=256) = 16
    depth: conint(ge=1, le=64) = 8
    tags: list[str] | None = None


@app.on_event("startup")
async def _startup() -> None:
    global _agent
    _agent = KolibriAgent()


def _parse_q(value: str | int) -> int:
    if isinstance(value, int):
        return value & 0xFFFFFFFFFFFFFFFF
    text = value.strip()
    if not text:
        raise ValueError("q must be non-empty")
    if text.isdigit():
        return int(text) & 0xFFFFFFFFFFFFFFFF
    digest = hashlib.sha256(text.encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "little")


@app.post("/api/agent/step")
async def agent_step(request: StepRequest):
    if _agent is None:
        raise HTTPException(status_code=503, detail="Agent not ready")
    try:
        q = _parse_q(request.q)
    except ValueError as error:
        raise HTTPException(status_code=400, detail=str(error)) from error
    result = await _agent.step(q, beam=request.beam, depth=request.depth, tags=request.tags)
    return result


@app.get("/api/agent/state")
async def agent_state():
    if _agent is None:
        raise HTTPException(status_code=503, detail="Agent not ready")
    return await _agent.snapshot()


__all__ = ["app"]
