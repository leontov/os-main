"""Symbolic embedding utilities for Kolibri."""

from __future__ import annotations

import hashlib
import math
import re
from dataclasses import dataclass
from typing import Iterable, List


TOKEN_RE = re.compile(r"[\w]+", re.UNICODE)


def _stable_seed(token: str, salt: int) -> int:
    digest = hashlib.sha256(f"{token}:{salt}".encode("utf-8")).digest()
    return int.from_bytes(digest[:8], "big", signed=False)


@dataclass(frozen=True)
class EmbeddingConfig:
    dimension: int = 48
    base_salt: int = 284765921


class SymbolicEmbeddingSpace:
    """Deterministically maps text tokens into dense numeric vectors."""

    def __init__(self, config: EmbeddingConfig | None = None) -> None:
        self.config = config or EmbeddingConfig()

    def _token_vector(self, token: str) -> List[float]:
        dim = self.config.dimension
        seed = self.config.base_salt
        values: List[float] = []
        for index in range(dim):
            seed_value = _stable_seed(token, seed + index)
            # Map to (-1, 1) range.
            scaled = (seed_value % 10_000_000) / 5_000_000.0 - 1.0
            values.append(scaled)
        return values

    def embed_tokens(self, tokens: Iterable[str]) -> List[float]:
        vectors = []
        for token in tokens:
            clean = token.lower()
            if not clean:
                continue
            vectors.append(self._token_vector(clean))
        if not vectors:
            return [0.0] * self.config.dimension
        accum = [0.0] * self.config.dimension
        for vector in vectors:
            for idx, value in enumerate(vector):
                accum[idx] += value
        norm = math.sqrt(sum(component * component for component in accum)) or 1.0
        return [component / norm for component in accum]

    def embed_text(self, text: str) -> List[float]:
        tokens = TOKEN_RE.findall(text or "")
        return self.embed_tokens(tokens)


__all__ = ["EmbeddingConfig", "SymbolicEmbeddingSpace"]
