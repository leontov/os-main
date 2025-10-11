"""LLM adapter scaffold for Kolibri — pluggable local model interface.

This module provides a tiny adapter interface so we can later plug real local
models (ONNX, HF transformers, etc.) while keeping the rest of the codebase
dependent only on a simple API.
"""
from __future__ import annotations

from typing import Any, Dict, Optional

from .inference import LocalRuleEngine, InferenceEngine


class LLMAdapter:
    """Abstract adapter interface.

    Implementations should expose `generate(prompt, **kwargs) -> Dict[str, Any]`.
    """

    def generate(self, prompt: str, **kwargs: Any) -> Dict[str, Any]:
        raise NotImplementedError()


class LocalAdapter(LLMAdapter):
    """Very small adapter that delegates to the rule-based LocalRuleEngine.

    Keeps results shaped like {'text': str, 'model': str, 'meta': {...}}
    so it can be swapped with a heavier adapter later.
    """

    def __init__(self, engine: Optional[InferenceEngine] = None) -> None:
        self.engine = engine or LocalRuleEngine()

    def generate(self, prompt: str, **kwargs: Any) -> Dict[str, Any]:
        out = self.engine.infer(prompt, **kwargs)
        # normalize shape
        return {"text": out.get("text", ""), "model": out.get("model", "local-adapter"), "meta": out}


def create_default_adapter() -> LLMAdapter:
    return LocalAdapter()
