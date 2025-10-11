"""Простой оркестратор роевого симулятора для локального тестирования Kolibri agents.

Запускает несколько инстансов `KolibriSim`, подключает `LocalKolibriAgent` к каждому,
выполняет несколько шагов и имитирует обмен знаний между случайно выбранными парами.

Использование:
    python scripts/simulate_swarm.py --nodes 5 --steps 20
"""
from __future__ import annotations

import argparse
import random
import statistics
from typing import List, Dict, Any
import csv
import json

from core.kolibri_sim import KolibriSim
from core.agent import LocalKolibriAgent


def run_swarm(nodes: int = 5, steps: int = 20, seed: int = 42, collect_steps: bool = False) -> Dict[str, Any]:
    rng = random.Random(seed)
    sims: List[KolibriSim] = [KolibriSim(zerno=seed + i) for i in range(nodes)]
    agents: List[LocalKolibriAgent] = [LocalKolibriAgent(name=f"agent-{i}") for i in range(nodes)]

    for sim, agent in zip(sims, agents):
        sim.ustanovit_agent(agent)

    telemetry: List[Dict[str, Any]] = []

    for step in range(steps):
        # run agent step on all nodes
        for sim in sims:
            sim.run_agent_step()

        # random peer exchanges
        pairs = []
        for _ in range(nodes // 2):
            a, b = rng.sample(sims, 2)
            pairs.append((a, b))
        for a, b in pairs:
            # exchange knowledge (znanija) between peers — vzjat_sostoyanie
            # returns a mapping of stimuli->responses, so use sinhronizaciya
            state = a.vzjat_sostoyanie()
            try:
                b.sinhronizaciya(state)
            except Exception:
                # fallback: if peer expects formula exchange, try that
                try:
                    b.exchange_formulas_with_peer(state)
                except Exception:
                    # ignore broken peers in simulation
                    continue

        if collect_steps:
            # snapshot metrics per step
            snapshot = {
                "step": step,
                "formula_counts": [len(s.formuly) for s in sims],
                "avg_formulas": statistics.mean([len(s.formuly) for s in sims]) if sims else 0,
            }
            telemetry.append(snapshot)

    # collect metrics
    formula_counts = [len(s.formuly) for s in sims]
    avg_formulas = statistics.mean(formula_counts) if formula_counts else 0
    fitnesses = []
    for s in sims:
        fitnesses.extend([r["fitness"] for r in s.formuly.values()])
    avg_fitness = statistics.mean(fitnesses) if fitnesses else 0.0

    result = {
        "nodes": nodes,
        "steps": steps,
        "avg_formulas": avg_formulas,
        "avg_fitness": avg_fitness,
    }
    if collect_steps:
        result["telemetry"] = telemetry
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--nodes", type=int, default=5)
    parser.add_argument("--steps", type=int, default=20)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--json", type=str, default="", help="Write result to JSON file")
    parser.add_argument("--csv", type=str, default="", help="Write step telemetry to CSV")
    args = parser.parse_args()

    res = run_swarm(nodes=args.nodes, steps=args.steps, seed=args.seed, collect_steps=bool(args.csv))
    print("Swarm run result:")
    for k, v in res.items():
        if k != "telemetry":
            print(f"  {k}: {v}")

    if args.json:
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump(res, fh, ensure_ascii=False, indent=2)

    if args.csv and res.get("telemetry"):
        with open(args.csv, "w", encoding="utf-8", newline="") as fh:
            writer = csv.writer(fh)
            node_count = args.nodes
            writer.writerow(["step"] + [f"node_{i}_formulas" for i in range(node_count)] + ["avg_formulas"])
            for row in res["telemetry"]:
                writer.writerow([row["step"]] + row["formula_counts"] + [row["avg_formulas"]])


if __name__ == "__main__":
    main()
