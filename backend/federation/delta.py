"""Delta serialization for Θ parameters with DP noise and HMAC signatures."""

from __future__ import annotations

import hashlib
import hmac
import json
from dataclasses import dataclass
from typing import Iterable, List

from ..feedback_service.theta import ThetaState


@dataclass
class ThetaDelta:
    theta: List[float]
    pi: List[float]
    rho: List[float]
    sigma: float
    updates: int
    ema_reward: float

    def to_json(self) -> dict[str, object]:
        return {
            "theta": self.theta,
            "pi": self.pi,
            "rho": self.rho,
            "sigma": self.sigma,
            "updates": self.updates,
            "ema_reward": self.ema_reward,
        }

    @classmethod
    def from_state(
        cls,
        state: ThetaState,
        *,
        noise_scale: float = 0.0,
    ) -> "ThetaDelta":
        return cls(
            theta=_add_noise(state.theta, noise_scale),
            pi=_add_noise(state.pi, noise_scale),
            rho=_add_noise(state.rho, noise_scale),
            sigma=state.sigma,
            updates=state.updates,
            ema_reward=state.ema_reward,
        )


def _add_noise(values: Iterable[float], scale: float) -> List[float]:
    if scale <= 0.0:
        return list(values)
    noise = []
    for index, value in enumerate(values):
        seed = hashlib.sha256(f"dp:{index}:{value}".encode("utf-8")).digest()
        fraction = int.from_bytes(seed[:8], "little") / float(1 << 53)
        noise.append(value + (fraction - 0.5) * 2 * scale)
    return noise


def sign_delta(delta: ThetaDelta, key: bytes) -> str:
    payload = json.dumps(delta.to_json(), separators=(",", ":"), ensure_ascii=False)
    signature = hmac.new(key, payload.encode("utf-8"), hashlib.sha256).hexdigest()
    return json.dumps({"payload": payload, "signature": signature}, ensure_ascii=False)


def verify_and_load(raw: str, key: bytes) -> ThetaDelta:
    parsed = json.loads(raw)
    payload = parsed["payload"]
    signature = parsed["signature"]
    expected = hmac.new(key, payload.encode("utf-8"), hashlib.sha256).hexdigest()
    if not hmac.compare_digest(signature, expected):
        raise ValueError("signature mismatch")
    data = json.loads(payload)
    return ThetaDelta(
        theta=list(map(float, data.get("theta", []))),
        pi=list(map(float, data.get("pi", []))),
        rho=list(map(float, data.get("rho", []))),
        sigma=float(data.get("sigma", 0.0)),
        updates=int(data.get("updates", 0)),
        ema_reward=float(data.get("ema_reward", 0.0)),
    )
