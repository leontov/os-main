"""PyTest-набор для KolibriSim: проверка цифрового мышления и роя."""

from __future__ import annotations

import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import pytest  # noqa: E402

from core.kolibri_sim import (  # noqa: E402
    KolibriSim,
    dec_hash,
    dolzhen_zapustit_repl,
    obnovit_soak_state,
    preobrazovat_tekst_v_cifry,
    sohranit_sostoyanie,
    vosstanovit_tekst_iz_cifr,
    zagruzit_sostoyanie,
    ZapisBloka,
    ZhurnalZapis,
)
from core.tracing import JsonLinesTracer  # noqa: E402


# --- Базовые тесты (T1–T7) -------------------------------------------------

def test_t1_text_roundtrip() -> None:
    tekst = "Колибри живёт цифрами"
    cifry = preobrazovat_tekst_v_cifry(tekst)
    assert cifry.isdigit()
    assert vosstanovit_tekst_iz_cifr(cifry) == tekst


def test_t2_teach_and_ask() -> None:
    sim = KolibriSim(zerno=42)
    sim.obuchit_svjaz("привет", "здравствуй")
    assert sim.sprosit("привет") == "здравствуй"
    assert sim.sprosit("неизвестно") == "..."


def test_t3_formula_evolution() -> None:
    sim = KolibriSim(zerno=1)
    f1 = sim.evolyuciya_formul("math")
    sim.ocenit_formulu(f1, 0.75)
    f2 = sim.evolyuciya_formul("math")
    assert f1 in sim.formuly
    assert f2 in sim.formuly
    assert sim.formuly[f1]["fitness"] > 0.0


def test_t4_genome_records_growth() -> None:
    sim = KolibriSim(zerno=5)
    start = len(sim.genom)
    sim.obuchit_svjaz("a", "b")
    sim.evolyuciya_formul("demo")
    assert len(sim.genom) > start
    assert sim.proverit_genom() is True


def test_t5_sync_merges_state() -> None:
    sim_a = KolibriSim(zerno=2)
    sim_b = KolibriSim(zerno=3)
    sim_a.obuchit_svjaz("a", "b")
    sim_b.obuchit_svjaz("c", "d")
    imported = sim_a.sinhronizaciya(sim_b.vzjat_sostoyanie())
    assert imported == 1
    assert sim_a.sprosit("c") == "d"


def test_t6_canvas_structure() -> None:
    sim = KolibriSim(zerno=9)
    sim.obuchit_svjaz("проба", "цифра")
    holst = sim.poluchit_canvas(glubina=4)
    assert len(holst) == 4
    assert all(len(stroka) == 10 for stroka in holst)


def test_t7_seed_determinism() -> None:
    sim_a = KolibriSim(zerno=99)
    sim_b = KolibriSim(zerno=99)
    assert sim_a.massiv_cifr(6) == sim_b.massiv_cifr(6)


# --- Дополнительные тесты (T8–T13) -----------------------------------------

def test_t8_dec_hash_deterministic() -> None:
    cifry = "0123456789" * 3
    assert dec_hash(cifry) == dec_hash(cifry)
    assert dec_hash(cifry).isdigit()


def test_t9_chat_commands() -> None:
    sim = KolibriSim(zerno=0)
    sim.obuchit_svjaz("стимул", "ответ")
    assert sim.dobrovolnaya_otpravka("стимул", "стимул") == "ответ"
    assert sim.dobrovolnaya_otpravka("серия", "3").isdigit()
    assert sim.dobrovolnaya_otpravka("число", "12a7") == "127"
    assert sim.dobrovolnaya_otpravka("выражение", "2+2*2") == "6"


def test_t10_repl_guard(monkeypatch: pytest.MonkeyPatch) -> None:
    env = {"KOLIBRI_REPL": "1"}
    assert dolzhen_zapustit_repl(env, True) is True
    assert dolzhen_zapustit_repl(env, False) is False
    assert dolzhen_zapustit_repl({}, True) is False


def test_t11_soak_progress(tmp_path: Path) -> None:
    sim = KolibriSim(zerno=7)
    state_path = tmp_path / "state.json"
    result = obnovit_soak_state(state_path, sim, minuti=2)

    events = result.get("events", 0)
    assert isinstance(events, int)
    assert events > 0

    assert state_path.exists()
    metrics = result.get("metrics", [])
    assert isinstance(metrics, list)
    assert len(metrics) == 2


def test_t12_population_and_parents() -> None:
    sim = KolibriSim(zerno=4)
    sim.zapustit_turniry(6)
    assert len(sim.populyaciya) <= sim.predel_populyacii
    assert any(sim.formuly[name]["parents"] for name in sim.formuly)


def test_t13_genome_verification_and_tamper() -> None:
    sim = KolibriSim(zerno=11)
    sim.obuchit_svjaz("a", "b")
    sim.evolyuciya_formul("tamper")
    assert sim.proverit_genom() is True
    # Нарушаем цепочку, обнуляя payload
    sim.genom[-1].payload = "000"
    assert sim.proverit_genom() is False


def test_t14_journal_rollover() -> None:
    sim = KolibriSim(zerno=123)
    sim.ustanovit_predel_zhurnala(5)
    for idx in range(12):
        sim.obuchit_svjaz(f"k{idx}", f"v{idx}")
    snapshot = sim.poluchit_zhurnal()
    assert snapshot["offset"] == 7
    assert len(snapshot["zapisi"]) == 5
    assert snapshot["zapisi"][0]["soobshenie"].startswith("k7")


def test_t15_soak_state_accumulates(tmp_path: Path) -> None:
    state_path = tmp_path / "soak.json"
    sim_a = KolibriSim(zerno=3)
    first = obnovit_soak_state(state_path, sim_a, minuti=1)
    events_first = first.get("events", 0)
    assert isinstance(events_first, int)
    assert events_first > 0

    sim_b = KolibriSim(zerno=3)
    second = obnovit_soak_state(state_path, sim_b, minuti=2)
    events_second = second.get("events", 0)
    assert isinstance(events_second, int)
    assert events_second > events_first
    metrics_second = second.get("metrics", [])
    metrics_first = first.get("metrics", [])
    assert isinstance(metrics_second, list)
    assert isinstance(metrics_first, list)
    assert len(metrics_second) >= len(metrics_first) + 2



# --- Трассировка и структурированные события -------------------------------

def test_tracer_receives_journal_events() -> None:
    sim = KolibriSim(zerno=21)

    class Collector:
        def __init__(self) -> None:
            self.records: list[tuple[ZhurnalZapis, ZapisBloka | None]] = []

        def zapisat(self, zapis: ZhurnalZapis, blok: ZapisBloka | None = None) -> None:
            self.records.append((zapis, blok))

    tracer = Collector()
    sim.ustanovit_tracer(tracer, vkljuchat_genom=True)

    sim.obuchit_svjaz("вопрос", "ответ")
    assert tracer.records, "tracer должен получать события"
    zapis, blok = tracer.records[0]
    assert zapis["tip"] == "TEACH"
    assert isinstance(zapis["soobshenie"], str)
    assert blok is not None

    sim.ustanovit_tracer(None)
    sim.sprosit("вопрос")


def test_default_jsonl_trace_created(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.chdir(tmp_path)
    monkeypatch.delenv("KOLIBRI_TRACE_PATH", raising=False)
    monkeypatch.delenv("KOLIBRI_LOG_DIR", raising=False)
    monkeypatch.delenv("KOLIBRI_TRACE_GENOME", raising=False)

    sim = KolibriSim(zerno=12)
    sim.obuchit_svjaz("alpha", "beta")

    trace_path = sim.poluchit_trace_path()
    assert trace_path is not None
    assert trace_path.exists()

    lines = [stroka for stroka in trace_path.read_text(encoding="utf-8").splitlines() if stroka.strip()]
    assert lines, "JSONL-файл должен содержать хотя бы одну запись"
    zapis = json.loads(lines[0])
    assert zapis["event"]["tip"] == "TEACH"


def test_trace_rotation_under_long_soak(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    trace_path = tmp_path / "trace.jsonl"
    monkeypatch.setenv("KOLIBRI_TRACE_PATH", str(trace_path))
    monkeypatch.delenv("KOLIBRI_TRACE_GENOME", raising=False)

    sim = KolibriSim(zerno=1)
    sim.ustanovit_predel_zhurnala(20)
    sim.zapustit_soak(minuti=40, sobytiya_v_minutu=6)

    snapshot = sim.poluchit_zhurnal()
    assert snapshot["offset"] > 0
    assert len(snapshot["zapisi"]) == 20

    lines = [stroka for stroka in trace_path.read_text(encoding="utf-8").splitlines() if stroka.strip()]
    assert len(lines) >= 40
    poslednyaya = json.loads(lines[-1])
    assert "event" in poslednyaya


def test_json_lines_tracer_writes(tmp_path: Path) -> None:
    sim = KolibriSim(zerno=8)
    trace_path = tmp_path / "trace" / "events.jsonl"
    tracer = JsonLinesTracer(trace_path, include_genome=True)
    sim.ustanovit_tracer(tracer, vkljuchat_genom=True)

    sim.obuchit_svjaz("alpha", "beta")
    sim.sprosit("alpha")

    contents = trace_path.read_text(encoding="utf-8").splitlines()
    assert len(contents) >= 2
    first_event = json.loads(contents[0])
    assert first_event["event"]["tip"] == "TEACH"
    assert "genome" in first_event



# --- Утилиты сохранения состояния -----------------------------------------

def test_state_roundtrip(tmp_path: Path) -> None:
    sim = KolibriSim(zerno=1)
    sim.obuchit_svjaz("alpha", "beta")
    state = {"knowledge": sim.vzjat_sostoyanie()}
    path = tmp_path / "dump.json"
    sohranit_sostoyanie(path, state)
    restored = zagruzit_sostoyanie(path)
    assert restored["knowledge"] == state["knowledge"]


if __name__ == "__main__":  # pragma: no cover
    pytest.main([__file__])
