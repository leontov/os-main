#!/usr/bin/env python3
"""Автоматическое склеивание конфликтов git с базовыми эвристиками Kolibri."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List

KONFLIKT_START = "<<<<<<<"
KONFLIKT_DELIM = "======="
KONFLIKT_END = ">>>>>>>"


def razobrat_konflikt(lines: List[str]) -> List[str]:
    """Объединяет конфликтные блоки, оставляя обе версии без маркеров."""
    rezultat: List[str] = []
    ours: List[str] = []
    theirs: List[str] = []
    sostoyanie = "normal"
    for stroka in lines:
        if stroka.startswith(KONFLIKT_START):
            sostoyanie = "ours"
            ours = []
            theirs = []
            continue
        if stroka.startswith(KONFLIKT_DELIM):
            sostoyanie = "theirs"
            continue
        if stroka.startswith(KONFLIKT_END):
            rezultat.extend(ours)
            if rezultat and rezultat[-1] != "\n":
                rezultat.append("\n")
            rezultat.extend(theirs)
            sostoyanie = "normal"
            continue
        if sostoyanie == "ours":
            ours.append(stroka)
        elif sostoyanie == "theirs":
            theirs.append(stroka)
        else:
            rezultat.append(stroka)
    return rezultat


def obrabotat_fajl(path: Path) -> Dict[str, object]:
    """Читает файл, устраняет конфликтные маркеры и возвращает отчёт."""
    soderzhimoe = path.read_text(encoding="utf-8")
    if KONFLIKT_START not in soderzhimoe:
        return {"file": str(path), "status": "clean"}
    stroki = soderzhimoe.splitlines(keepends=True)
    novye = razobrat_konflikt(stroki)
    path.write_text("".join(novye), encoding="utf-8")
    return {"file": str(path), "status": "resolved"}


def nayti_fajly(root: Path) -> List[Path]:
    """Возвращает список отслеживаемых файлов с потенциальными конфликтами."""
    return [
        path
        for path in root.rglob("*")
        if path.is_file() and not path.name.startswith(".")
    ]


def postroit_otchet(root: Path) -> Dict[str, object]:
    """Формирует итоговый отчёт по всем обработанным файлам."""
    rezultaty: List[Dict[str, object]] = []
    for fajl in nayti_fajly(root):
        try:
            rezultaty.append(obrabotat_fajl(fajl))
        except UnicodeDecodeError:
            rezultaty.append({"file": str(fajl), "status": "skipped"})
    return {"files": rezultaty}


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description="Автоконфликт Kolibri")
    parser.add_argument("--report", type=Path, default=None, help="путь для JSON-отчёта")
    args = parser.parse_args(argv)
    koren = Path.cwd()
    otchet = postroit_otchet(koren)
    if args.report:
        args.report.write_text(json.dumps(otchet, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(otchet, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
