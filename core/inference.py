"""Минимальный локальный inference pipeline для Kolibri.

Содержит интерфейс InferenceEngine и простую реализацию LocalRuleEngine,
которая использует эвристики агента для генерации ответов (без внешних моделей).
"""
from __future__ import annotations

from typing import Any, Dict


class InferenceEngine:
    """Абстрактный интерфейс для движка вывода."""

    def infer(self, prompt: str, **kwargs: Any) -> Dict[str, Any]:
        """Выполняет вывод и возвращает словарь с результатами."""

        raise NotImplementedError()


class LocalRuleEngine(InferenceEngine):
    """Простейшая правило-ориентированная заглушка для локального вывода.

    Используется когда внешние модели не нужны — генерирует ответ на основе коротких правил.
    """

    def infer(self, prompt: str, **kwargs: Any) -> Dict[str, Any]:
        p = prompt.strip().lower()
        if p.startswith("стимул"):
            parts = prompt.split("\n", 1)
            if len(parts) == 2 and "->" in parts[1]:
                key, val = parts[1].split("->", 1)
                return {"text": f"taught:{key.strip()}->{val.strip()}", "model": "local-rule", "reason": "stimulus-teach"}
        if "1+1" in p or p.startswith("выражение"):
            return {"text": "2", "model": "local-rule", "reason": "expression"}
        return {"text": prompt, "model": "local-rule", "reason": "echo"}


__all__ = ["InferenceEngine", "LocalRuleEngine"]
