"""Инструменты структурированного логирования для KolibriSim."""

from __future__ import annotations

import json
from dataclasses import asdict, is_dataclass
from pathlib import Path
from typing import Any, Dict, TYPE_CHECKING

if TYPE_CHECKING:
    from .kolibri_sim import ZapisBloka, ZhurnalZapis


class JsonLinesTracer:
    """Сохраняет события журнала KolibriSim в JSON Lines файле."""

    def __init__(self, path: Path, *, include_genome: bool = False) -> None:
        self._path = Path(path)
        self._include_genome = include_genome

    def zapisat(self, zapis: "ZhurnalZapis", blok: "ZapisBloka | None" = None) -> None:
        zapic: Dict[str, Any] = {"event": zapis}
        if self._include_genome and blok is not None:
            zapic["genome"] = asdict(blok) if is_dataclass(blok) else blok  # type: ignore[arg-type]
        line = json.dumps(zapic, ensure_ascii=False, sort_keys=True)
        self._path.parent.mkdir(parents=True, exist_ok=True)
        with self._path.open("a", encoding="utf-8") as output:
            output.write(line)
            output.write("\n")


__all__ = ["JsonLinesTracer"]
