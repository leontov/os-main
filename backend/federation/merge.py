"""Merger for federated Theta deltas."""

from __future__ import annotations

from typing import Iterable, List

from ..feedback_service.theta import ThetaState
from .delta import ThetaDelta


def merge_deltas(state: ThetaState, deltas: Iterable[ThetaDelta]) -> ThetaState:
    total_weight = 1.0
    theta_acc = list(state.theta)
    pi_acc = list(state.pi)
    rho_acc = list(state.rho)
    sigma = state.sigma
    ema_reward = state.ema_reward

    for delta in deltas:
        weight = max(1.0, float(delta.updates))
        total_weight += weight
        _accumulate(theta_acc, delta.theta, weight)
        _accumulate(pi_acc, delta.pi, weight)
        _accumulate(rho_acc, delta.rho, weight)
        sigma += delta.sigma * weight
        ema_reward += delta.ema_reward * weight

    if total_weight <= 1.0:
        return state

    return ThetaState(
        theta=_normalize(theta_acc, total_weight),
        pi=_normalize(pi_acc, total_weight),
        rho=_normalize(rho_acc, total_weight),
        updates=state.updates + 1,
        ema_reward=ema_reward / total_weight,
        sigma=max(0.05, sigma / total_weight),
    )


def _accumulate(base: List[float], values: List[float], weight: float) -> None:
    if not values:
        return
    if not base:
        base.extend([0.0] * len(values))
    if len(base) < len(values):
        base.extend([0.0] * (len(values) - len(base)))
    for index, value in enumerate(values):
        base[index] += value * weight


def _normalize(values: List[float], weight: float) -> List[float]:
    if not values:
        return []
    return [value / weight for value in values]
