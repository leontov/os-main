"""Адаптер θ: обновляет параметры Kolibri Nano по пользовательской обратной связи."""

from __future__ import annotations

import asyncio
import json
import logging
import math
import os
import random
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Final, List, TYPE_CHECKING

from core.memory import LongTermMemory
from core.representations import SymbolicEmbeddingSpace

if TYPE_CHECKING:  # pragma: no cover - подсказки типов на этапе разработки
    from .schemas import FeedbackRecord
else:
    FeedbackRecord = Any  # type: ignore[assignment]

logger = logging.getLogger(__name__)

try:
    _EMBEDDING_SPACE = SymbolicEmbeddingSpace()
    _LTM = LongTermMemory(_EMBEDDING_SPACE)
except Exception as error:  # pragma: no cover - защитный путь на случай импортных ошибок
    logger.warning("LongTermMemory недоступна: %s", error)
    _EMBEDDING_SPACE = None
    _LTM = None


@dataclass
class ThetaState:
    """Хранит параметры Колибри Nano и состояние адаптации."""

    theta: List[float] = field(default_factory=list)
    pi: List[float] = field(default_factory=list)
    rho: List[float] = field(default_factory=list)
    updates: int = 0
    ema_reward: float = 0.0
    sigma: float = 0.2


class ThetaUpdater:
    """Инкрементально корректирует θ на основе сигналов обратной связи."""

    _DEFAULT_THETA: Final[List[float]] = [1.0, 0.3, -0.2, 0.12]
    _DEFAULT_PI: Final[List[float]] = [0.0] * 8
    _DEFAULT_RHO: Final[List[float]] = [0.0] * 8

    def __init__(
        self,
        path: Path | None = None,
        *,
        learning_rate: float = 0.05,
        decay: float = 0.002,
        l2: float = 1e-3,
        clip: float = 2.5,
        max_theta: int = 16,
    ) -> None:
        self._path = self._resolve_path(path)
        self._csv_path = self._path.with_suffix(".csv")
        self._learning_rate = learning_rate
        self._decay = decay
        self._l2 = l2
        self._clip = clip
        self._max_theta = max_theta
        self._total_budget = 96
        self._pi_dim = min(len(self._DEFAULT_PI), max_theta)
        self._rho_dim = min(len(self._DEFAULT_RHO), max_theta)
        self._lock: asyncio.Lock | None = None
        self._state: ThetaState | None = None
        self._bin_path = self._path.with_suffix(".bin")

    @staticmethod
    def _resolve_path(path: Path | None) -> Path:
        if path is None:
            env_path = os.getenv("KNP_THETA_STATE_PATH", "data/knp_theta.json")
            path_obj = Path(env_path)
        else:
            path_obj = Path(path)
        if not path_obj.is_absolute():
            path_obj = Path.cwd() / path_obj
        return path_obj

    async def _ensure_state(self) -> ThetaState:
        if self._state is None:
            state = await asyncio.to_thread(self._load_state_sync)
            self._state = self._normalise_state(state)
        return self._state

    def _load_state_sync(self) -> ThetaState:
        try:
            with self._path.open("r", encoding="utf-8") as handle:
                payload = json.load(handle)
        except FileNotFoundError:
            return ThetaState(
                theta=list(self._DEFAULT_THETA),
                pi=list(self._DEFAULT_PI),
                rho=list(self._DEFAULT_RHO),
                sigma=0.2,
            )
        except json.JSONDecodeError as error:
            logger.warning("Не удалось прочитать θ из %s: %s", self._path, error)
            return ThetaState(
                theta=list(self._DEFAULT_THETA),
                pi=list(self._DEFAULT_PI),
                rho=list(self._DEFAULT_RHO),
                sigma=0.2,
            )

        raw_theta = payload.get("theta", [])
        theta = [float(value) for value in raw_theta if isinstance(value, (int, float))]
        raw_pi = payload.get("pi", [])
        pi = [float(value) for value in raw_pi if isinstance(value, (int, float))]
        raw_rho = payload.get("rho", [])
        rho = [float(value) for value in raw_rho if isinstance(value, (int, float))]
        updates = int(payload.get("updates", 0))
        ema_reward = float(payload.get("ema_reward", 0.0))
        sigma = float(payload.get("sigma", 0.2))
        return ThetaState(theta=theta, pi=pi, rho=rho, updates=updates, ema_reward=ema_reward, sigma=sigma)

    def _normalise_state(self, state: ThetaState) -> ThetaState:
        theta = [value for value in state.theta if isinstance(value, (int, float)) and math.isfinite(value)]
        if not theta:
            theta = list(self._DEFAULT_THETA)
        if len(theta) > self._max_theta:
            theta = theta[: self._max_theta]
        # гарантируем как минимум одну координату
        if not theta:
            theta = [1.0]
        state.theta = theta
        remaining = max(0, self._total_budget - len(theta))
        pi_dim = min(self._pi_dim, remaining // 2)
        rho_dim = min(self._rho_dim, max(0, remaining - pi_dim))

        pi = [value for value in getattr(state, "pi", []) if isinstance(value, (int, float)) and math.isfinite(value)]
        rho = [value for value in getattr(state, "rho", []) if isinstance(value, (int, float)) and math.isfinite(value)]

        if pi_dim:
            if not pi:
                pi = list(self._DEFAULT_PI[:pi_dim])
            if len(pi) < pi_dim:
                pi.extend([0.0] * (pi_dim - len(pi)))
            elif len(pi) > pi_dim:
                pi = pi[:pi_dim]
        else:
            pi = []

        if rho_dim:
            if not rho:
                rho = list(self._DEFAULT_RHO[:rho_dim])
            if len(rho) < rho_dim:
                rho.extend([0.0] * (rho_dim - len(rho)))
            elif len(rho) > rho_dim:
                rho = rho[:rho_dim]
        else:
            rho = []

        state.pi = pi
        state.rho = rho
        if not math.isfinite(getattr(state, "sigma", 0.2)) or state.sigma <= 0.0:
            state.sigma = 0.2
        state.sigma = max(0.01, min(state.sigma, 1.0))
        return state

    def _save_state_sync(self, state: ThetaState) -> None:
        data = {
            "theta": state.theta,
            "pi": state.pi,
            "rho": state.rho,
            "updates": state.updates,
            "ema_reward": state.ema_reward,
            "sigma": state.sigma,
        }
        self._path.parent.mkdir(parents=True, exist_ok=True)
        with self._path.open("w", encoding="utf-8") as handle:
            json.dump(data, handle, ensure_ascii=False, indent=2)
        csv_parts = [
            "θ:" + ",".join(f"{value:.12g}" for value in state.theta),
            "π:" + ",".join(f"{value:.12g}" for value in state.pi),
            "ρ:" + ",".join(f"{value:.12g}" for value in state.rho),
            f"σ:{state.sigma:.12g}",
        ]
        csv_line = "\n".join(csv_parts) + "\n"
        with self._csv_path.open("w", encoding="utf-8") as handle:
            handle.write(csv_line)
        try:
            with self._bin_path.open("wb") as handle:
                for value in state.theta + state.pi + state.rho + [state.sigma]:
                    handle.write(struct.pack("<d", value))
        except OSError as error:
            logger.warning("Не удалось записать params.bin: %s", error)

    async def update(self, record: FeedbackRecord) -> None:
        """Обновляет θ, не прерывая обработку запроса даже при ошибках."""

        lock = await self._ensure_lock()
        async with lock:
            state = await self._ensure_state()
            theta = state.theta
            n_theta = len(theta)
            signal, metrics = self._collect_features(record)
            basis = self._basis_values(signal, n_theta)
            reward = self._extract_reward(record)
            if reward == 0.0:
                logger.debug("Пропускаем обновление θ: неопознанный рейтинг %r", getattr(record, "rating", None))
                return
            self._record_memory(record)
            prediction = sum(value * feature for value, feature in zip(theta, basis))
            learning_rate = self._effective_learning_rate(state.updates)
            error = reward - prediction

            self._apply_vector_update(theta, basis, error, learning_rate)
            pi_features = self._pi_features(metrics, reward, len(state.pi))
            rho_features = self._rho_features(metrics, reward, prediction, error, state)
            if state.pi:
                self._apply_vector_update(state.pi, pi_features, reward, learning_rate * 0.6)
            if state.rho:
                self._apply_vector_update(state.rho, rho_features, reward, learning_rate * 0.4)

            self._gradient_consistency(basis, error, state)
            state.sigma = max(0.05, state.sigma * 0.995)

            state.ema_reward = 0.9 * state.ema_reward + 0.1 * reward
            state.updates += 1

            await asyncio.to_thread(self._save_state_sync, state)
            logger.debug(
                "theta обновлена: updates=%d reward=%.3f lr=%.5f -> %s",
                state.updates,
                reward,
                learning_rate,
                ",".join(f"{value:.4f}" for value in theta),
            )

    async def current_theta(self) -> List[float]:
        """Возвращает копию текущего вектора θ."""

        lock = await self._ensure_lock()
        async with lock:
            state = await self._ensure_state()
            return list(state.theta)

    async def current_state(self) -> ThetaState:
        lock = await self._ensure_lock()
        async with lock:
            state = await self._ensure_state()
            return ThetaState(
                theta=list(state.theta),
                pi=list(state.pi),
                rho=list(state.rho),
                updates=state.updates,
                ema_reward=state.ema_reward,
                sigma=state.sigma,
            )

    async def persist_state(self, state: ThetaState) -> None:
        lock = await self._ensure_lock()
        async with lock:
            self._state = state
            await asyncio.to_thread(self._save_state_sync, state)

    @property
    def state_path(self) -> Path:
        return self._path

    @property
    def csv_path(self) -> Path:
        return self._csv_path

    async def _ensure_lock(self) -> asyncio.Lock:
        if self._lock is None:
            self._lock = asyncio.Lock()
        return self._lock

    def _effective_learning_rate(self, updates: int) -> float:
        return self._learning_rate / (1.0 + self._decay * max(updates, 0))

    def _basis_values(self, x: float, n_theta: int) -> List[float]:
        if n_theta <= 0:
            return []
        features: List[float] = [x]
        if n_theta == 1:
            return features

        kmax = (n_theta - 1) // 2
        z = 2.0 * x - 1.0
        for k in range(1, kmax + 1):
            features.append(self._chebyshev_T(z, k))
            if len(features) >= n_theta:
                break
            features.append(math.sin(math.pi * k * x))
            if len(features) >= n_theta:
                break

        base_len = 1 + 2 * kmax
        if base_len < n_theta:
            features.append(1.0)  # свободный член

        while len(features) < n_theta:
            features.append(0.0)

        return features[:n_theta]

    @staticmethod
    def _chebyshev_T(z: float, k: int) -> float:
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

    def _collect_features(self, record: FeedbackRecord) -> tuple[float, dict[str, float]]:
        assistant_text = (getattr(record, "assistant_message", "") or "")
        assistant_length = min(len(assistant_text) / 600.0, 1.0)
        user_component = 0.0
        user_text = getattr(record, "user_message", None)
        if user_text:
            user_component = min(len(user_text) / 400.0, 1.0)
        comment_component = 0.1 if getattr(record, "comment", None) else 0.0
        mode_component = 0.5
        mode_text = getattr(record, "mode", None)
        if mode_text:
            checksum = sum(ord(ch) for ch in mode_text)
            mode_component = (checksum % 997) / 997.0

        context_strength = 0.0
        if _LTM is not None and assistant_text:
            matches = _LTM.query(assistant_text, top_k=3)
            if matches:
                context_strength = sum(max(0.0, score) for _, score in matches) / len(matches)

        signal = (
            0.45 * assistant_length
            + 0.2 * mode_component
            + 0.15 * user_component
            + comment_component
            + 0.2 * context_strength
        )
        signal = min(max(signal, 0.0), 1.0)
        metrics = {
            "assistant_length": assistant_length,
            "user_component": user_component,
            "comment_component": comment_component,
            "mode_component": mode_component,
            "context_strength": context_strength,
            "signal": signal,
        }
        return signal, metrics

    def _record_memory(self, record: FeedbackRecord) -> None:
        if _LTM is None or _EMBEDDING_SPACE is None:
            return
        assistant_text = getattr(record, "assistant_message", None)
        user_text = getattr(record, "user_message", None)
        meta_common = {
            "conversation_id": str(getattr(record, "conversation_id", "")),
            "message_id": str(getattr(record, "message_id", "")),
        }
        rating = getattr(record, "rating", None)
        if hasattr(rating, "value"):
            meta_common["rating"] = str(rating.value)
        elif rating is not None:
            meta_common["rating"] = str(rating)
        if assistant_text:
            _LTM.append(assistant_text, meta={**meta_common, "role": "assistant"})
        if user_text:
            _LTM.append(user_text, meta={**meta_common, "role": "user"})

    def _apply_vector_update(self, vector: List[float], features: List[float], signal: float, lr: float) -> None:
        for idx, feature in enumerate(features):
            if idx >= len(vector):
                break
            prior = vector[idx]
            gradient = signal * feature
            adjusted = prior * (1.0 - lr * self._l2) + lr * gradient
            if adjusted > self._clip:
                adjusted = self._clip
            elif adjusted < -self._clip:
                adjusted = -self._clip
            vector[idx] = adjusted

    def _pi_features(self, metrics: dict[str, float], reward: float, dimension: int) -> List[float]:
        base = [
            metrics.get("assistant_length", 0.0),
            metrics.get("user_component", 0.0),
            metrics.get("mode_component", 0.0),
            metrics.get("context_strength", 0.0),
            metrics.get("signal", 0.0),
            reward,
        ]
        return self._pad_features(base, dimension)

    def _rho_features(
        self,
        metrics: dict[str, float],
        reward: float,
        prediction: float,
        error: float,
        state: ThetaState,
    ) -> List[float]:
        base = [
            reward,
            error,
            prediction,
            metrics.get("context_strength", 0.0),
            state.sigma,
            metrics.get("signal", 0.0),
            1.0,
        ]
        return self._pad_features(base, len(state.rho))

    @staticmethod
    def _pad_features(base: List[float], dimension: int) -> List[float]:
        if dimension <= 0:
            return []
        if len(base) >= dimension:
            return base[:dimension]
        return base + [0.0] * (dimension - len(base))

    def _gradient_consistency(self, basis: List[float], error: float, state: ThetaState) -> None:
        if not basis or state.sigma <= 0.0:
            return
        samples = (4, 8, 16)
        estimates: List[List[float]] = []
        for sample in samples:
            accum = [0.0] * len(basis)
            for _ in range(sample):
                for idx, feature in enumerate(basis):
                    direction = 1.0 if random.random() > 0.5 else -1.0
                    accum[idx] += (error / state.sigma) * feature * direction
            estimates.append([value / sample for value in accum])
        corr_4_8 = self._correlation(estimates[0], estimates[1])
        corr_8_16 = self._correlation(estimates[1], estimates[2])
        if corr_4_8 < 0.9 or corr_8_16 < 0.9:
            logger.debug(
                "Низкая корреляция градиентных оценок: corr(4,8)=%.3f corr(8,16)=%.3f",
                corr_4_8,
                corr_8_16,
            )

    @staticmethod
    def _correlation(a: List[float], b: List[float]) -> float:
        if len(a) != len(b) or not a:
            return 1.0
        mean_a = sum(a) / len(a)
        mean_b = sum(b) / len(b)
        num = 0.0
        den_a = 0.0
        den_b = 0.0
        for va, vb in zip(a, b):
            da = va - mean_a
            db = vb - mean_b
            num += da * db
            den_a += da * da
            den_b += db * db
        if den_a == 0.0 or den_b == 0.0:
            return 1.0
        return num / math.sqrt(den_a * den_b)

    def _extract_reward(self, record: FeedbackRecord) -> float:
        rating = getattr(record, "rating", None)
        if rating is None:
            return 0.0
        label: str
        if hasattr(rating, "value"):
            label = str(getattr(rating, "value"))
        else:
            label = str(rating)
        label = label.lower()
        if label == "useful":
            return 1.0
        if label == "not_useful":
            return -1.0
        return 0.0


_theta_updater: ThetaUpdater | None = None
_theta_lock = asyncio.Lock()


async def get_theta_updater() -> ThetaUpdater:
    """Возвращает синглтон-экземпляр обновляющего θ."""

    global _theta_updater

    if _theta_updater is None:
        async with _theta_lock:
            if _theta_updater is None:
                _theta_updater = ThetaUpdater()
    return _theta_updater


__all__ = ["ThetaUpdater", "ThetaState", "get_theta_updater"]
