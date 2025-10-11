"""Quick integration smoke test for the swarm orchestrator."""
from scripts.simulate_swarm import run_swarm


def test_swarm_short():
    res = run_swarm(nodes=3, steps=5, seed=7)
    assert res["nodes"] == 3
    assert res["steps"] == 5
    assert res["avg_formulas"] >= 0
    assert isinstance(res["avg_fitness"], float)
