"""Persistent and working memory modules for Kolibri agents."""

from __future__ import annotations

import json
import math
import time
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Deque, Iterable, List, Optional

from .representations import SymbolicEmbeddingSpace


def _cosine_similarity(a: Iterable[float], b: Iterable[float]) -> float:
    dot = 0.0
    norm_a = 0.0
    norm_b = 0.0
    for va, vb in zip(a, b):
        dot += va * vb
        norm_a += va * va
        norm_b += vb * vb
    if norm_a == 0.0 or norm_b == 0.0:
        return 0.0
    return dot / (math.sqrt(norm_a) * math.sqrt(norm_b))


@dataclass
class MemoryRecord:
    text: str
    embedding: List[float]
    timestamp: float
    meta: dict[str, str] = field(default_factory=dict)
    tags: List[str] = field(default_factory=list)
    ttl: Optional[float] = None

    def to_json(self) -> dict[str, object]:
        return {
            "text": self.text,
            "embedding": self.embedding,
            "timestamp": self.timestamp,
            "meta": self.meta,
            "tags": self.tags,
            "ttl": self.ttl,
        }

    @classmethod
    def from_json(cls, payload: dict[str, object]) -> "MemoryRecord":
        return cls(
            text=str(payload.get("text", "")),
            embedding=[float(x) for x in payload.get("embedding", [])],
            timestamp=float(payload.get("timestamp", 0.0)),
            meta=dict(payload.get("meta", {})),
            tags=list(payload.get("tags", []) or []),
            ttl=float(payload["ttl"]) if payload.get("ttl") is not None else None,
        )


class LongTermMemory:
    """Stores embeddings persistently and allows similarity queries."""

    def __init__(
        self,
        embeddings: SymbolicEmbeddingSpace,
        *,
        path: Path | str | None = None,
        max_entries: int = 2048,
        ttl_seconds: Optional[float] = None,
    ) -> None:
        self.embeddings = embeddings
        self.path = Path(path or "data/long_term_memory.jsonl")
        self.max_entries = max_entries
        self.records: List[MemoryRecord] = []
        self.ttl_seconds = ttl_seconds
        self._load()

    def _load(self) -> None:
        if not self.path.exists():
            return
        try:
            with self.path.open("r", encoding="utf-8") as handle:
                for line in handle:
                    line = line.strip()
                    if not line:
                        continue
                    record = MemoryRecord.from_json(json.loads(line))
                    self.records.append(record)
        except FileNotFoundError:
            return
        self._prune_expired()

    def _rewrite(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("w", encoding="utf-8") as handle:
            for record in self.records:
                handle.write(json.dumps(record.to_json(), ensure_ascii=False) + "\n")

    def _ensure_capacity(self) -> None:
        if len(self.records) <= self.max_entries:
            return
        # Keep the most recent entries.
        self.records = self.records[-self.max_entries :]
        self._rewrite()

    def append(self, text: str, *, meta: Optional[dict[str, str]] = None) -> MemoryRecord:
        if not text:
            raise ValueError("Memory text must be non-empty")
        embedding = self.embeddings.embed_text(text)
        ttl = self.ttl_seconds
        meta_copy = dict(meta or {})
        tags = list(meta_copy.pop("tags", []))
        record = MemoryRecord(
            text=text,
            embedding=embedding,
            timestamp=time.time(),
            meta=meta_copy,
            tags=tags,
            ttl=ttl,
        )
        self.records.append(record)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(record.to_json(), ensure_ascii=False) + "\n")
        self._prune_expired()
        self._ensure_capacity()
        return record

    def query(self, text: str, *, top_k: int = 3) -> List[tuple[MemoryRecord, float]]:
        if not self.records:
            return []
        self._prune_expired()
        query_embedding = self.embeddings.embed_text(text)
        scored = [
            (record, _cosine_similarity(query_embedding, record.embedding)) for record in self.records
        ]
        scored.sort(key=lambda item: item[1], reverse=True)
        return scored[:top_k]

    def _prune_expired(self) -> None:
        now = time.time()
        changed = False
        ttl_default = self.ttl_seconds
        filtered: List[MemoryRecord] = []
        for record in self.records:
            ttl = record.ttl if record.ttl is not None else ttl_default
            if ttl is not None and record.timestamp + ttl < now:
                changed = True
                continue
            filtered.append(record)
        if changed:
            self.records = filtered
            self._rewrite()


@dataclass
class WorkingMemorySlot:
    q: int
    tau: float
    kappa: float
    weight: float
    tags: List[str] = field(default_factory=list)


class WorkingMemoryBuffer:
    """Ring buffer capturing recent numeric impulses with decay."""

    def __init__(self, capacity: int = 32, decay: float = 0.9) -> None:
        self.capacity = capacity
        self.decay = decay
        self.slots: Deque[WorkingMemorySlot] = deque(maxlen=capacity)

    def add(self, q: int, tau: float, kappa: float, *, tags: Optional[List[str]] = None) -> None:
        tags = list(tags or [])
        self._decay()
        self.slots.appendleft(WorkingMemorySlot(q=q, tau=tau, kappa=kappa, weight=1.0, tags=tags))

    def _decay(self) -> None:
        updated: Deque[WorkingMemorySlot] = deque(maxlen=self.capacity)
        for slot in self.slots:
            decayed_weight = slot.weight * self.decay
            if decayed_weight < 1e-5:
                continue
            updated.append(WorkingMemorySlot(q=slot.q, tau=slot.tau, kappa=slot.kappa, weight=decayed_weight, tags=slot.tags))
        self.slots = updated

    def snapshot(self) -> List[WorkingMemorySlot]:
        return list(self.slots)

    def clear(self) -> None:
        self.slots.clear()

    def as_dict(self) -> List[dict[str, object]]:
        return [
            {
                "q": slot.q,
                "tau": slot.tau,
                "kappa": slot.kappa,
                "weight": slot.weight,
                "tags": list(slot.tags),
            }
            for slot in self.slots
        ]


__all__ = ["LongTermMemory", "MemoryRecord", "WorkingMemoryBuffer", "WorkingMemorySlot"]
