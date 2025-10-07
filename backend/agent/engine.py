from __future__ import annotations

import math
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional

from core.memory import LongTermMemory, WorkingMemoryBuffer
from core.representations import SymbolicEmbeddingSpace

from ..feedback_service.theta import ThetaState, ThetaUpdater


def _splitmix64(value: int) -> int:
    value = (value + 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
    z = value
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9 & 0xFFFFFFFFFFFFFFFF
    z = (z ^ (z >> 27)) * 0x94D049BB133111EB & 0xFFFFFFFFFFFFFFFF
    return (z ^ (z >> 31)) & 0xFFFFFFFFFFFFFFFF


def _u64_to_unit(value: int) -> float:
    mant = ((value >> 11) | 1) & 0x1FFFFFFFFFFFFF
    return max(1e-16, min((mant / float(1 << 53)), 1.0 - 1e-16))


def _chi(seed: int) -> float:
    a = _splitmix64(seed)
    b = _splitmix64(a ^ 0xD1B54A32D192ED03)
    u = _u64_to_unit(b)
    t = 1.0 - abs(2.0 * u - 1.0)
    l = 4.0 * t * (1.0 - t)
    return max(1e-16, min(0.5 * (t + l), 1.0 - 1e-16))


def _chebyshev(z: float, k: int) -> float:
    if k == 0:
        return 1.0
    if k == 1:
        return z
    tkm2 = 1.0
    tkm1 = z
    tk = z
    for _ in range(2, k + 1):
        tk = 2.0 * z * tkm1 - tkm2
        tkm2, tkm1 = tkm1, tk
    return tk


def _phi(x: float, theta: List[float]) -> float:
    if not theta:
        return x
    x = max(1e-16, min(x, 1.0 - 1e-16))
    y = theta[0] * x
    kmax = (len(theta) - 1) // 2
    z = 2.0 * x - 1.0
    for k in range(1, kmax + 1):
        Tk = _chebyshev(z, k)
        s = math.sin(math.pi * k * x)
        idx = 2 * k - 1
        y += theta[idx] * Tk + theta[idx + 1] * s
    core_len = 1 + kmax * 2
    if len(theta) > core_len:
        y += theta[core_len]
    return y


def _score(q: int, value: float) -> float:
    qn = _u64_to_unit(q)
    vn = max(1e-16, min(value, 1.0 - 1e-16))
    return -abs(vn - qn)


@dataclass
class TraceNode:
    level: int
    identifier: int
    chi: float
    phi: float
    score: float

    def to_dict(self) -> Dict[str, float | int]:
        return {
            "level": self.level,
            "identifier": self.identifier,
            "chi": self.chi,
            "phi": self.phi,
            "score": self.score,
        }


class KolibriAgent:
    """High level agent orchestrating Kolibri Nano steps."""

    def __init__(self, *, theta_path: str | None = None) -> None:
        self.seed_base = 0xD1B54A32D192ED03
        self.theta_updater = ThetaUpdater(path=Path(theta_path) if theta_path else None)
        self.embedding = SymbolicEmbeddingSpace()
        self.long_memory = LongTermMemory(self.embedding, ttl_seconds=7 * 24 * 3600)
        self.working_memory = WorkingMemoryBuffer(capacity=48, decay=0.88)

    async def step(
        self,
        q: int,
        *,
        beam: int = 16,
        depth: int = 8,
        tags: Optional[List[str]] = None,
    ) -> Dict[str, Any]:
        state = await self.theta_updater.current_state()
        modulated_q = self._modulate_query(q, state.pi)
        beam = max(1, min(beam, 256))
        depth = max(1, min(depth, 64))
        best, trace = self._infer_with_trace(modulated_q, state, beam, depth)
        self.working_memory.add(q, tau=best.chi, kappa=best.phi, tags=["step"] + (tags or []))
        self.long_memory.append(
            f"step q={q} score={best.score:.4f}",
            meta={"tip": "step", "tags": ["step"] + (tags or [])},
        )
        return {
            "q": q,
            "modulated_q": modulated_q,
            "chi": best.chi,
            "phi": best.phi,
            "score": best.score,
            "best_id": best.identifier,
            "beam": beam,
            "depth": depth,
            "trace": [node.to_dict() for node in trace],
            "working_memory": self.working_memory.as_dict(),
            "theta": {
                "theta": state.theta,
                "pi": state.pi,
                "rho": state.rho,
                "sigma": state.sigma,
                "updates": state.updates,
                "ema_reward": state.ema_reward,
            },
            "timestamp": time.time(),
        }

    async def snapshot(self) -> Dict[str, Any]:
        state = await self.theta_updater.current_state()
        return {
            "theta": state.theta,
            "pi": state.pi,
            "rho": state.rho,
            "sigma": state.sigma,
            "updates": state.updates,
            "ema_reward": state.ema_reward,
            "working_memory": self.working_memory.as_dict(),
        }

    def _infer_with_trace(
        self,
        q: int,
        state: ThetaState,
        beam: int,
        depth: int,
    ) -> tuple[TraceNode, List[TraceNode]]:
        theta = state.theta
        base_seed = self._seed_with_policy(state.rho)
        current: List[TraceNode] = []
        for d in range(10):
            identifier = _splitmix64(base_seed ^ q ^ d)
            chi = _chi(identifier)
            phi_value = _phi(chi, theta)
            score = _score(q, phi_value)
            current.append(TraceNode(level=0, identifier=identifier, chi=chi, phi=phi_value, score=score))
        current.sort(key=lambda node: node.score, reverse=True)
        current = current[:beam]
        trace: List[TraceNode] = list(current)
        for level in range(1, depth):
            next_nodes: List[TraceNode] = []
            for node in current:
                base = _splitmix64(node.identifier ^ base_seed ^ (level * 0x9E37))
                for d in range(10):
                    identifier = _splitmix64(base ^ d)
                    chi = _chi(identifier)
                    phi_value = _phi(chi, theta)
                    score = _score(q, phi_value)
                    next_nodes.append(TraceNode(level=level, identifier=identifier, chi=chi, phi=phi_value, score=score))
                    if len(next_nodes) >= beam:
                        break
                if len(next_nodes) >= beam:
                    break
            if not next_nodes:
                break
            next_nodes.sort(key=lambda node: node.score, reverse=True)
            next_nodes = next_nodes[:beam]
            trace.extend(next_nodes)
            current = next_nodes
        best = max(trace, key=lambda node: node.score)
        return best, trace

    def _modulate_query(self, q: int, pi: List[float]) -> int:
        if not pi:
            return q
        influence = sum(pi) / max(1, len(pi))
        delta = int(abs(influence) * 1e6) & 0xFFFFFFFFFFFFFFFF
        return (q ^ delta) & 0xFFFFFFFFFFFFFFFF

    def _seed_with_policy(self, rho: List[float]) -> int:
        if not rho:
            return self.seed_base
        factor = sum(abs(v) for v in rho) / max(1, len(rho))
        mix = int(factor * 1e6) & 0xFFFFFFFFFFFFFFFF
        return self.seed_base ^ mix


__all__ = ["KolibriAgent", "TraceNode"]
