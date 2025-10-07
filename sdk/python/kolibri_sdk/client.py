from __future__ import annotations

import datetime as _dt
from dataclasses import dataclass
from typing import Iterable, List, Optional

import httpx

@dataclass
class TraceNode:
    level: int
    identifier: int
    chi: float
    phi: float
    score: float

@dataclass
class WorkingMemorySlot:
    q: int
    tau: float
    kappa: float
    weight: float
    tags: List[str]

@dataclass
class AgentStep:
    q: int
    modulated_q: int
    chi: float
    phi: float
    score: float
    best_id: int
    beam: int
    depth: int
    trace: List[TraceNode]
    working_memory: List[WorkingMemorySlot]
    theta: dict
    timestamp: _dt.datetime

@dataclass
class AgentState:
    theta: List[float]
    pi: List[float]
    rho: List[float]
    sigma: float
    updates: int
    ema_reward: float
    working_memory: List[WorkingMemorySlot]


class KolibriAgentClient:
    """Python client for Kolibri agent REST API."""

    def __init__(self, base_url: str = "http://127.0.0.1:8056") -> None:
        self._client = httpx.Client(base_url=base_url.rstrip("/"))

    def close(self) -> None:
        self._client.close()

    def step(
        self,
        *,
        q: int | str,
        beam: int = 16,
        depth: int = 8,
        tags: Optional[Iterable[str]] = None,
        timeout: Optional[float] = 10.0,
    ) -> AgentStep:
        payload = {"q": q, "beam": beam, "depth": depth}
        if tags:
            payload["tags"] = list(tags)
        response = self._client.post("/api/agent/step", json=payload, timeout=timeout)
        response.raise_for_status()
        data = response.json()
        return AgentStep(
            q=data["q"],
            modulated_q=data["modulated_q"],
            chi=data["chi"],
            phi=data["phi"],
            score=data["score"],
            best_id=data["best_id"],
            beam=data["beam"],
            depth=data["depth"],
            trace=[TraceNode(**node) for node in data.get("trace", [])],
            working_memory=[WorkingMemorySlot(**slot) for slot in data.get("working_memory", [])],
            theta=data.get("theta", {}),
            timestamp=_dt.datetime.fromtimestamp(data.get("timestamp", 0.0)),
        )

    def state(self, timeout: Optional[float] = 5.0) -> AgentState:
        response = self._client.get("/api/agent/state", timeout=timeout)
        response.raise_for_status()
        data = response.json()
        return AgentState(
            theta=data.get("theta", []),
            pi=data.get("pi", []),
            rho=data.get("rho", []),
            sigma=data.get("sigma", 0.0),
            updates=data.get("updates", 0),
            ema_reward=data.get("ema_reward", 0.0),
            working_memory=[WorkingMemorySlot(**slot) for slot in data.get("working_memory", [])],
        )

    def __enter__(self) -> "KolibriAgentClient":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()
