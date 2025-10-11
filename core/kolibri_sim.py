"""Модуль высокоуровневой симуляции узла «Колибри» для Python-тестов."""

from __future__ import annotations

import ast
import dataclasses
import hashlib
import hmac
import json
import os
import random
import time
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional, Protocol, Sequence, TypedDict, cast

from .kolibri_script.genome import (
    KolibriGenomeLedger,
    SecretsConfig,
    load_secrets_config,
)
from .memory import LongTermMemory
from .representations import SymbolicEmbeddingSpace
from .tracing import JsonLinesTracer


class FormulaRecord(TypedDict):
    """Структура формулы, эволюционирующей в KolibriSim."""

    kod: str
    fitness: float
    parents: List[str]
    context: str


class ZhurnalZapis(TypedDict):
    """Структурированная запись журнала событий симуляции."""

    tip: str
    soobshenie: str
    metka: float


class ZhurnalSnapshot(TypedDict):
    """Срез журнала, включая смещение отброшенных записей."""

    offset: int
    zapisi: List[ZhurnalZapis]


class ZhurnalTracer(Protocol):
    """Интерфейс обработчика структурированных событий журнала."""

    def zapisat(self, zapis: ZhurnalZapis, blok: "ZapisBloka | None" = None) -> None:
        """Получает уведомление о новой записи журнала и соответствующем блоке."""


class FormulaZapis(TypedDict):
    """Запись о формуле, хранящаяся в популяции KolibriSim."""

    kod: str
    fitness: float
    parents: List[str]
    context: str


class MetricRecord(TypedDict):
    """Метрика одного шага soak-прогона."""

    minute: int
    formula: str
    fitness: float
    genome: int


MetricEntry = MetricRecord


class SoakResult(TypedDict):
    """Результат выполнения soak-сессии."""

    events: int
    metrics: List[MetricRecord]


class SoakState(TypedDict, total=False):
    """Состояние накопленных soak-прогонов, сохранённое на диске."""

    events: int
    metrics: List[MetricRecord]


@dataclasses.dataclass
class ZapisBloka:
    """Хранит блок цифрового генома, включая ссылки на предыдущие состояния."""

    nomer: int
    pred_hash: str
    payload: str
    hmac_summa: str
    itogovy_hash: str


def preobrazovat_tekst_v_cifry(tekst: str) -> str:
    """Переводит UTF-8 текст в поток десятичных цифр по правилам Kolibri."""

    dannye = tekst.encode("utf-8")
    return "".join(f"{bayt:03d}" for bayt in dannye)


def vosstanovit_tekst_iz_cifr(cifry: str) -> str:
    """Восстанавливает строку из десятичного представления."""

    if len(cifry) % 3 != 0:
        raise ValueError("длина цепочки цифр должна делиться на три")
    bayty = bytearray(int(cifry[ind:ind + 3]) for ind in range(0, len(cifry), 3))
    return bayty.decode("utf-8")


def dec_hash(cifry: str) -> str:
    """Формирует десятичный хеш SHA-256, устойчивый к платформенным различиям."""

    digest = hashlib.sha256(cifry.encode("utf-8")).digest()
    return "".join(str(bajt % 10) for bajt in digest)


def dolzhen_zapustit_repl(peremennye: Mapping[str, str], est_tty: bool) -> bool:
    """Проверяет, следует ли запускать REPL: нужен флаг KOLIBRI_REPL=1 и наличие TTY."""

    return peremennye.get("KOLIBRI_REPL") == "1" and est_tty


def _poschitat_hmac(klyuch: bytes, pred_hash: str, payload: str) -> str:
    """Возвращает HMAC-SHA256 в десятичном представлении."""

    soobshenie = (pred_hash + payload).encode("utf-8")
    hex_kod = hmac.new(klyuch, soobshenie, hashlib.sha256).hexdigest()
    return preobrazovat_tekst_v_cifry(hex_kod)


class KolibriSim:
    """Минималистичная симуляция узла Kolibri для сценариев CI и unit-тестов."""

    def __init__(
        self,
        zerno: int = 0,
        hmac_klyuch: Optional[bytes] = None,
        *,
        trace_path: "Path | str | None" = None,
        trace_include_genome: Optional[bool] = None,
        genome_path: "Path | str | None" = None,
        secrets_config: "SecretsConfig | None" = None,
        secrets_path: "Path | str | None" = None,
    ) -> None:
        self.zerno = zerno
        self.generator = random.Random(zerno)
        self.hmac_klyuch: bytes | str = hmac_klyuch or b"kolibri-hmac"
        self.zhurnal: List[ZhurnalZapis] = []
        self.predel_zhurnala = 256
        self._zhurnal_sdvig = 0
        self.znanija: Dict[str, str] = {}
        self.formuly: Dict[str, FormulaRecord] = {}
        self.populyaciya: List[str] = []
        self.predel_populyacii = 24
        self.genom: List[ZapisBloka] = []
        self._tracer: Optional[ZhurnalTracer] = None
        self._tracer_include_genome = False
        self._trace_path: Optional[Path] = None
        self._genome_writer: Optional[KolibriGenomeLedger] = None
        self.embedding_space = SymbolicEmbeddingSpace()
        self.long_memory = LongTermMemory(self.embedding_space, ttl_seconds=7 * 24 * 3600)

        if genome_path is not None:
            secrets = secrets_config or load_secrets_config(secrets_path)
            self._genome_writer = KolibriGenomeLedger(Path(genome_path), secrets)

        self._nastroit_avto_tracer(trace_path, trace_include_genome)

        genesis = self._sozdanie_bloka("GENESIS", {"seed": zerno})
        writer = self._genome_writer
        if writer is not None and not writer.records:
            writer.append(
                dataclasses.asdict(genesis),
                {"tip": "GENESIS", "soobshenie": f"seed={zerno}", "metka": time.time()},
            )

    # --- Вспомогательные методы ---
    def _sozdanie_bloka(self, tip: str, dannye: Mapping[str, object]) -> ZapisBloka:
        """Кодирует событие в цифровой геном и возвращает созданный блок."""

        zapis = {
            "tip": tip,
            "dannye": dict(dannye),
            "metka": len(self.genom),
        }
        payload = preobrazovat_tekst_v_cifry(json.dumps(zapis, ensure_ascii=False, sort_keys=True))
        pred_hash = self.genom[-1].itogovy_hash if self.genom else dec_hash("kolibri-genesis")
        hmac_summa = _poschitat_hmac(self._poluchit_klyuch(), pred_hash, payload)
        itogovy_hash = dec_hash(payload + hmac_summa + pred_hash)
        blok = ZapisBloka(len(self.genom), pred_hash, payload, hmac_summa, itogovy_hash)
        self.genom.append(blok)
        return blok

    def _poluchit_klyuch(self) -> bytes:
        """Гарантирует наличие HMAC-ключа в байтовом виде."""

        if isinstance(self.hmac_klyuch, str):
            self.hmac_klyuch = self.hmac_klyuch.encode("utf-8")
        return self.hmac_klyuch

    def _nastroit_avto_tracer(
        self,
        trace_path: "Path | str | None",
        trace_include_genome: Optional[bool],
    ) -> None:
        """Автоматически подключает JSONL-трассер, если он не отключён."""

        path = self._vybrat_trace_path(trace_path)
        if path is None:
            return

        include_genome = self._vybrat_trace_genome(trace_include_genome)
        tracer = JsonLinesTracer(path, include_genome=include_genome)
        self.ustanovit_tracer(tracer, vkljuchat_genom=include_genome)

    def _vybrat_trace_path(self, trace_path: "Path | str | None") -> Optional[Path]:
        """Определяет путь к JSONL-журналу с учётом переменных окружения."""

        if not self._env_flag(os.getenv("KOLIBRI_TRACE"), default=True):
            return None

        if trace_path is not None:
            if str(trace_path).strip() == "":
                return None
            return Path(trace_path)

        env_path = os.getenv("KOLIBRI_TRACE_PATH")
        if env_path is not None:
            stripped = env_path.strip()
            if stripped.lower() in {"", "0", "false", "no", "off"}:
                return None
            return Path(stripped)

        log_dir_env = os.getenv("KOLIBRI_LOG_DIR")
        base_dir = Path(log_dir_env) if log_dir_env else Path.cwd()
        return base_dir / "kolibri_trace.jsonl"

    @staticmethod
    def _env_flag(value: Optional[str], *, default: bool) -> bool:
        """Интерпретирует переменную окружения как логический флаг."""

        if value is None:
            return default
        return value.strip().lower() not in {"", "0", "false", "no", "off"}

    def _vybrat_trace_genome(self, trace_include_genome: Optional[bool]) -> bool:
        """Определяет, нужно ли добавлять блоки генома в журнал."""

        if trace_include_genome is not None:
            return trace_include_genome
        env_value = os.getenv("KOLIBRI_TRACE_GENOME")
        return self._env_flag(env_value, default=False)

    def _registrirovat(self, tip: str, soobshenie: str) -> None:
        """Добавляет запись в оперативный журнал действий."""

        zapis: ZhurnalZapis = {
            "tip": tip,
            "soobshenie": soobshenie,
            "metka": time.time(),
        }
        self.zhurnal.append(zapis)
        if len(self.zhurnal) > self.predel_zhurnala:
            sdvig = len(self.zhurnal) - self.predel_zhurnala
            del self.zhurnal[:sdvig]
            self._zhurnal_sdvig += sdvig

        blok = self._sozdanie_bloka(tip, zapis)
        writer = self._genome_writer
        if writer is not None:
            writer.append(dataclasses.asdict(blok), zapis)
        tracer = self._tracer
        if tracer is not None:
            blok_dlya_tracinga = blok if self._tracer_include_genome else None
            try:
                tracer.zapisat(zapis, blok_dlya_tracinga)
            except Exception as oshibka:  # pragma: no cover - ошибки трассера должны быть видимы
                raise RuntimeError("KolibriSim tracer не смог обработать событие") from oshibka

    # --- Базовые операции обучения ---
    def obuchit_svjaz(self, stimul: str, otvet: str) -> None:
        """Добавляет ассоциацию в память и фиксирует событие в геноме."""

        self.znanija[stimul] = otvet
        self._registrirovat("TEACH", f"{stimul}->{otvet}")
        self.long_memory.append(
            f"ассоциация: {stimul} → {otvet}",
            meta={"tip": "association", "tags": ["teach", "memory"]},
        )

    def sprosit(self, stimul: str) -> str:
        """Возвращает ответ из памяти или многоточие, если знания нет."""

        if stimul in self.znanija:
            otvet = self.znanija[stimul]
            self._registrirovat("ASK", f"{stimul}->{otvet}")
            return otvet

        candidates = self.long_memory.query(stimul, top_k=1)
        if candidates and candidates[0][1] >= 0.6:
            match, score = candidates[0]
            self._registrirovat("ASK", f"{stimul}->LTM[{score:.2f}]")
            return match.text

        self._registrirovat("ASK", f"{stimul}->...")
        return "..."

    def dobrovolnaya_otpravka(self, komanda: str, argument: str) -> str:
        """Обрабатывает команды чата KolibriScript, используя русские ключевые слова."""

        komanda = komanda.strip().lower()
        if komanda == "стимул":
            self.long_memory.append(
                f"запрос: {argument}",
                meta={"tip": "query", "tags": ["stimulus"]},
            )
            return self.sprosit(argument)
        if komanda == "серия":
            chislo = max(0, min(9, int(argument) if argument.isdigit() else 0))
            posledovatelnost = "".join(str((ind + chislo) % 10) for ind in range(10))
            self._registrirovat("SERIES", posledovatelnost)
            self.long_memory.append(
                f"серия: начало {chislo} → {posledovatelnost}",
                meta={"tip": "series", "tags": ["series"]},
            )
            return posledovatelnost
        if komanda == "число":
            cifry = "".join(symbol for symbol in argument if symbol.isdigit())
            self._registrirovat("NUMBER", cifry)
            self.long_memory.append(
                f"числовой импульс: {cifry or '0'}",
                meta={"tip": "number", "tags": ["number"]},
            )
            return cifry or "0"
        if komanda == "выражение":
            znachenie = self._bezopasnoe_vychislenie(argument)
            rezultat = str(znachenie)
            self._registrirovat("EXPR", rezultat)
            self.long_memory.append(
                f"выражение: {argument} = {rezultat}",
                meta={"tip": "expression", "tags": ["expression"]},
            )
            return rezultat
        raise ValueError(f"неизвестная команда: {komanda}")

    def _bezopasnoe_vychislenie(self, vyrazhenie: str) -> int:
        """Вычисляет арифметическое выражение через AST, исключая опасные конструкции."""

        uzel = ast.parse(vyrazhenie, mode="eval")
        return int(self._evaluate_ast(uzel.body))

    def _evaluate_ast(self, uzel: ast.AST) -> int:
        """Рекурсивный интерпретатор арифметических выражений для команд REPL."""

        if isinstance(uzel, ast.BinOp) and isinstance(uzel.op, (ast.Add, ast.Sub, ast.Mult, ast.Pow)):
            levy = self._evaluate_ast(uzel.left)
            pravy = self._evaluate_ast(uzel.right)
            if isinstance(uzel.op, ast.Add):
                return levy + pravy
            if isinstance(uzel.op, ast.Sub):
                return levy - pravy
            if isinstance(uzel.op, ast.Mult):
                return levy * pravy
            return levy ** pravy
        if isinstance(uzel, ast.UnaryOp) and isinstance(uzel.op, (ast.UAdd, ast.USub)):
            znachenie = self._evaluate_ast(uzel.operand)
            return znachenie if isinstance(uzel.op, ast.UAdd) else -znachenie
        if isinstance(uzel, ast.Constant) and isinstance(uzel.value, (int, float)):
            return int(uzel.value)
        raise ValueError("поддерживаются только простые арифметические выражения")

    # --- Эволюция формул ---
    def evolyuciya_formul(self, kontekst: str) -> str:
        """Создаёт новую формулу, базируясь на имеющихся родителях."""

        rod_stroki: Sequence[str] = list(self.formuly.keys())
        roditeli: List[str]
        if rod_stroki:
            k = min(2, len(rod_stroki))
            roditeli = self.generator.sample(list(rod_stroki), k=k)
        else:
            roditeli = []
        mnozhitel = self.generator.randint(1, 9)
        smeshchenie = self.generator.randint(0, 9)
        kod = f"f(x)={mnozhitel}*x+{smeshchenie}"
        nazvanie = f"F{len(self.formuly) + 1:04d}"
        zapis: FormulaZapis = {
            "kod": kod,
            "fitness": 0.0,
            "parents": roditeli,
            "context": kontekst,
        }
        self.formuly[nazvanie] = zapis
        self.populyaciya.append(nazvanie)
        if len(self.populyaciya) > self.predel_populyacii:
            self.populyaciya.pop(0)
        self._registrirovat("FORMULA", f"{nazvanie}:{kod}")
        return nazvanie

    def ocenit_formulu(self, nazvanie: str, uspeh: float) -> float:
        """Обновляет фитнес формулы и возвращает новое значение."""

        zapis = self.formuly[nazvanie]
        tekushchij = zapis["fitness"]
        novoe_znachenie = 0.6 * uspeh + 0.4 * tekushchij
        zapis["fitness"] = novoe_znachenie
        self._registrirovat("FITNESS", f"{nazvanie}:{novoe_znachenie:.3f}")
        return novoe_znachenie

    def zapustit_turniry(self, kolichestvo: int) -> None:
        """Имитация нескольких раундов эволюции с неизменной численностью популяции."""

        for _ in range(kolichestvo):
            nazvanie = self.evolyuciya_formul("tournament")
            self.ocenit_formulu(nazvanie, self.generator.random())

    # --- Цифровой геном и синхронизация ---
    def proverit_genom(self) -> bool:
        """Проверяет целостность генома и корректность HMAC-цепочки."""

        pred_hash = dec_hash("kolibri-genesis")
        for blok in self.genom:
            if blok.pred_hash != pred_hash:
                return False
            ozhidaemyj_hmac = _poschitat_hmac(self._poluchit_klyuch(), pred_hash, blok.payload)
            if blok.hmac_summa != ozhidaemyj_hmac:
                return False
            ozhidaemyj_hash = dec_hash(blok.payload + blok.hmac_summa + pred_hash)
            if blok.itogovy_hash != ozhidaemyj_hash:
                return False
            pred_hash = blok.itogovy_hash
        return True

    def poluchit_genom_slovar(self) -> List[Dict[str, str]]:
        """Возвращает список словарей для сериализации генома."""

        return [dataclasses.asdict(blok) for blok in self.genom]

    def sinhronizaciya(self, sostoyanie: Mapping[str, str]) -> int:
        """Импортирует отсутствующие знания и возвращает счётчик новых связей."""

        dobavleno = 0
        for stimul, otvet in sostoyanie.items():
            if stimul not in self.znanija:
                self.znanija[stimul] = otvet
                dobavleno += 1
        self._registrirovat("SYNC", f"imported={dobavleno}")
        return dobavleno

    def poluchit_canvas(self, glubina: int = 3) -> List[List[int]]:
        """Формирует числовое представление фрактальной памяти для визуализации."""

        osnova = "".join(preobrazovat_tekst_v_cifry(znachenie) for znachenie in self.znanija.values())
        if not osnova:
            osnova = "0123456789"
        sloi: List[List[int]] = []
        for uroven in range(glubina):
            start = (uroven * 10) % len(osnova)
            segment = osnova[start:start + 10]
            if len(segment) < 10:
                segment = (segment + osnova)[:10]
            sloi.append([int(simbol) for simbol in segment])
        return sloi

    def vzjat_sostoyanie(self) -> Dict[str, str]:
        """Возвращает копию текущих знаний для синхронизации."""

        return dict(self.znanija)

    # --- Agent integration hooks (lightweight and non-invasive) ---
    def ustanovit_agent(self, agent: object) -> None:
        """Привязывает агент к симулятору (неинвазивно сохраняет ссылку)."""

        self._agent = agent
        self._registrirovat("AGENT", f"set={getattr(agent, 'name', str(agent))}")

    def run_agent_step(self) -> Optional[object]:
        """Выполнить один шаг агента: observe -> decide -> act.

        Возвращает результат agent.act или None. Если агент не задан — возбуждает RuntimeError.
        """

        if not hasattr(self, "_agent") or self._agent is None:
            raise RuntimeError("agent not set; call ustanovit_agent() first")

        try:
            obs = self._agent.observe(self)
        except Exception:
            obs = {
                "znanija_count": len(self.znanija),
                "populyaciya": list(self.populyaciya),
                "zhurnal": list(self.zhurnal[-5:]),
            }

        try:
            decision = self._agent.decide(obs)
        except Exception as e:  # pragma: no cover
            self._registrirovat("AGENT_ERROR", f"decide failed: {e}")
            return None

        try:
            result = self._agent.act(self, decision)
            self._registrirovat("AGENT_STEP", f"action={getattr(decision, 'meta', {})}")
            return result
        except Exception as e:  # pragma: no cover
            self._registrirovat("AGENT_ERROR", f"act failed: {e}")
            return None

    def run_agent_loop(self, steps: int = 1, delay: float = 0.0) -> List[Optional[object]]:
        """Запустить агент несколько шагов подряд и вернуть список результатов."""

        results: List[Optional[object]] = []
        for _ in range(max(0, int(steps))):
            res = self.run_agent_step()
            results.append(res)
            if delay and delay > 0:
                time.sleep(delay)
        return results

    def save_agent_state(self, path: "Path | str") -> bool:
        """Попросить агент сохранить своё состояние по пути, если поддерживает save_state."""

        if not hasattr(self, "_agent") or self._agent is None:
            return False
        try:
            if hasattr(self._agent, "save_state"):
                self._agent.save_state(str(path))
                self._registrirovat("AGENT_SAVE", str(path))
                return True
        except Exception:
            return False
        return False

    def load_agent_state(self, path: "Path | str") -> bool:
        """Попытка загрузить состояние агента из файла через LocalKolibriAgent.load_state."""

        try:
            from .agent import LocalKolibriAgent

            agent = LocalKolibriAgent.load_state(str(path))
            self.ustanovit_agent(agent)
            self._registrirovat("AGENT_LOAD", str(path))
            return True
        except Exception:
            return False

    def exchange_formulas_with_peer(self, peer_formulas: Mapping[str, Any]) -> int:
        """Простейшая синхронизация формул: импорт отсутствующих записей и возврат числа добавленных."""

        added = 0
        for name, record in peer_formulas.items():
            if name not in self.formuly:
                self.formuly[name] = record  # type: ignore[assignment]
                self.populyaciya.append(name)
                added += 1
        if added:
            self._registrirovat("EXCH_FORMULA", f"added={added}")
        return added

    def ustanovit_predel_zhurnala(self, predel: int) -> None:
        """Задаёт максимальный размер журнала и немедленно усечает избыток."""

        if predel < 1:
            raise ValueError("предельный размер журнала должен быть положительным")
        self.predel_zhurnala = predel
        if len(self.zhurnal) > predel:
            sdvig = len(self.zhurnal) - predel
            del self.zhurnal[:sdvig]
            self._zhurnal_sdvig += sdvig

    def poluchit_zhurnal(self) -> ZhurnalSnapshot:
        """Возвращает снимок журнала с информацией о отброшенных записях."""

        return {"offset": self._zhurnal_sdvig, "zapisi": list(self.zhurnal)}

    def ustanovit_tracer(self, tracer: Optional[ZhurnalTracer], *, vkljuchat_genom: bool = False) -> None:
        """Настраивает обработчик событий журнала и управление блоками генома."""

        self._tracer = tracer
        self._tracer_include_genome = bool(tracer) and vkljuchat_genom
        if tracer is None:
            self._trace_path = None
        elif isinstance(tracer, JsonLinesTracer):
            self._trace_path = tracer._path  # type: ignore[attr-defined]
        else:
            self._trace_path = None

    def poluchit_trace_path(self) -> Optional[Path]:
        """Возвращает путь к активному JSONL-журналу, если он настроен."""

        return self._trace_path

    def massiv_cifr(self, kolichestvo: int) -> List[int]:
        """Генерирует детерминированную последовательность цифр на основе зерна."""

        return [self.generator.randint(0, 9) for _ in range(kolichestvo)]

    def zapustit_soak(self, minuti: int, sobytiya_v_minutu: int = 4) -> SoakResult:
        """Имитация длительного прогона: создаёт формулы и записи генома."""

        minuti = max(0, minuti)
        nachalnyj_razmer = len(self.genom)
        metrika: List[MetricRecord] = []

        for minuta in range(minuti):
            nazvanie = self.evolyuciya_formul("soak")
            rezultat = self.ocenit_formulu(nazvanie, self.generator.random())
            metrika.append(
                {
                    "minute": minuta,
                    "formula": nazvanie,
                    "fitness": rezultat,
                    "genome": len(self.genom),
                }
            )
            dobavlyaemyh = max(1, sobytiya_v_minutu - 1)
            for idx in range(dobavlyaemyh):
                stimul = f"stim-{minuta}-{idx}"
                otvet = f"resp-{self.generator.randint(0, 999):03d}"
                self.obuchit_svjaz(stimul, otvet)

        return {"events": len(self.genom) - nachalnyj_razmer, "metrics": metrika}


def sohranit_sostoyanie(path: Path, sostoyanie: Mapping[str, Any]) -> None:
    """Сохраняет состояние в JSON с переводом текстов в цифровой слой."""

    serializovannoe = {
        k: preobrazovat_tekst_v_cifry(json.dumps(v, ensure_ascii=False, sort_keys=True))
        for k, v in sostoyanie.items()
    }
    path.write_text(json.dumps(serializovannoe, ensure_ascii=False, sort_keys=True), encoding="utf-8")


def zagruzit_sostoyanie(path: Path) -> Dict[str, Any]:
    """Загружает состояние из цифровой формы и восстанавливает структуру."""

    if not path.exists():
        return {}
    dannye = json.loads(path.read_text(encoding="utf-8"))
    rezultat: Dict[str, Any] = {}
    for k, v in dannye.items():
        tekst = vosstanovit_tekst_iz_cifr(v)
        rezultat[k] = json.loads(tekst)
    return rezultat


def obnovit_soak_state(path: Path, sim: KolibriSim, minuti: int) -> SoakState:
    """Читает, дополняет и сохраняет состояние длительных прогонов."""

    tekuschee_raw = zagruzit_sostoyanie(path)
    tekuschee: SoakState = cast(SoakState, tekuschee_raw)
    itogi = sim.zapustit_soak(minuti)

    metrics_obj = tekuschee.get("metrics")
    if isinstance(metrics_obj, list):
        metrics = cast(List[MetricRecord], metrics_obj)
    else:
        metrics = []
        tekuschee["metrics"] = metrics
    metrics.extend(itogi["metrics"])

    events_prev = tekuschee.get("events")
    if not isinstance(events_prev, int):
        events_prev = 0
    tekuschee["events"] = events_prev + itogi["events"]

    sohranit_sostoyanie(path, tekuschee)
    return tekuschee


__all__ = [
    "KolibriSim",
    "ZapisBloka",
    "FormulaRecord",
    "MetricEntry",
    "MetricRecord",
    "SoakResult",
    "SoakState",
    "ZhurnalSnapshot",
    "ZhurnalTracer",
    "preobrazovat_tekst_v_cifry",
    "vosstanovit_tekst_iz_cifr",
    "dec_hash",
    "dolzhen_zapustit_repl",
    "sohranit_sostoyanie",
    "zagruzit_sostoyanie",
    "obnovit_soak_state",
]
