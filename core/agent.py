"""Local Kolibri Agent — resilient implementation used in tests.

This file provides a compact, test-friendly LocalKolibriAgent compatible with
`KolibriSim` usage in tests and the swarm orchestrator.
"""
from __future__ import annotations

import time
from dataclasses import dataclass
from typing import Any, Dict, Optional

from .inference import InferenceEngine, LocalRuleEngine
from .llm_adapter import LLMAdapter, create_default_adapter


@dataclass
class AgentResult:
    text: str
    meta: Dict[str, Any]


class LocalKolibriAgent:
    def __init__(self, name: str = "local-agent", decision_latency: float = 0.01, engine: Optional[InferenceEngine] = None, adapter: Optional[LLMAdapter] = None) -> None:
        self.name = name
        self.decision_latency = decision_latency
        self.tick = 0
        self.engine: InferenceEngine = engine or LocalRuleEngine()
        self.adapter: LLMAdapter = adapter or create_default_adapter()
        self.last_inference: Optional[Dict[str, Any]] = None

    def observe(self, sim: object) -> Dict[str, Any]:
        return {
            "znanija_count": len(getattr(sim, "znanija", {})),
            "populyaciya": list(getattr(sim, "populyaciya", [])[-3:]),
            "last_journal": list(getattr(sim, "zhurnal", [])[-3:]),
            "tick": self.tick,
        }

    def decide(self, context: Dict[str, Any]) -> AgentResult:
        time.sleep(self.decision_latency)
        self.tick += 1
        znanija = context.get("znanija_count", 0)

        if znanija < 3:
            text = f"стимул\nauto{self.tick}->value{self.tick}"
            meta = {"action": "teach", "reason": "bootstrapping"}
            return AgentResult(text=text, meta=meta)

        if self.tick % 4 == 0:
            text = f"эволюция\nagent-{self.tick}"
            meta = {"action": "evolve_formula", "reason": "periodic"}
            return AgentResult(text=text, meta=meta)

        if self.tick % 3 == 0:
            prompt = f"стимул\npattern->{self.tick}"
            meta = {"action": "infer", "reason": "rule_engine", "tick": self.tick}
            return AgentResult(text=prompt, meta=meta)

        if znanija < 5:
            text = f"серия\n{self.tick % 10}"
            meta = {"action": "produce_series", "reason": "growing_knowledge"}
            return AgentResult(text=text, meta=meta)

        pop = context.get("populyaciya", [])
        if not pop:
            text = "выражение\n1+1"
            meta = {"action": "compute", "reason": "no_formula"}
            return AgentResult(text=text, meta=meta)

        text = f"эволюция\nagent-{self.tick}"
        meta = {"action": "evolve_formula", "reason": "regular_tick"}
        return AgentResult(text=text, meta=meta)

    def act(self, sim: object, result: AgentResult) -> Optional[str]:
        try:
            action = result.meta.get("action")
            if action == "teach":
                parts = result.text.split("\n", 1)
                if len(parts) == 2 and "->" in parts[1]:
                    stimul, otvet = parts[1].split("->", 1)
                    sim.obuchit_svjaz(stimul.strip(), otvet.strip())
                    return f"taught:{stimul.strip()}->{otvet.strip()}"
            if action == "compute":
                parts = result.text.split("\n", 1)
                if len(parts) == 2:
                    return sim.dobrovolnaya_otpravka(parts[0], parts[1])
            if action == "produce_series":
                parts = result.text.split("\n", 1)
                arg = parts[1] if len(parts) == 2 else "0"
                return sim.dobrovolnaya_otpravka("серия", arg)
            if action == "evolve_formula":
                parts = result.text.split("\n", 1)
                kontekst = parts[1] if len(parts) == 2 else "agent"
                nazvanie = sim.evolyuciya_formul(kontekst)
                score = getattr(sim, "generator", None)
                uspeh = score.random() if score is not None else 0.5
                sim.ocenit_formulu(nazvanie, uspeh)
                return f"evolved:{nazvanie}:{uspeh:.3f}"
            if action == "infer":
                # prefer adapter when available
                try:
                    out = self.adapter.generate(result.text, tick=self.tick, meta=result.meta)
                    self.last_inference = out.get("meta") if isinstance(out.get("meta"), dict) else out
                    return out.get("text", "")
                except Exception:
                    payload = self.engine.infer(result.text, tick=self.tick, meta=result.meta)
                    self.last_inference = payload
                    return payload.get("text", "")
        except Exception:
            return None
        return None

    def save_state(self, path: str) -> None:
        import json

        data = {"name": self.name, "tick": self.tick, "decision_latency": self.decision_latency, "last_inference": self.last_inference}
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)

    @classmethod
    def load_state(cls, path: str) -> "LocalKolibriAgent":
        import json

        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        agent = cls(name=data.get("name", "local-agent"), decision_latency=data.get("decision_latency", 0.01))
        agent.tick = int(data.get("tick", 0))
        agent.last_inference = data.get("last_inference")
        return agent


__all__ = ["LocalKolibriAgent", "AgentResult"]
