#!/usr/bin/env python3
"""Служебный скрипт для оставления комментариев в PR и отчётов сторожевого воркфлоу."""

from __future__ import annotations

import argparse
import json
import os
import sys
from typing import Any, Dict, List
from urllib.error import HTTPError
from urllib.request import Request, urlopen

API_BASE = "https://api.github.com"


def _get_headers(token: str | None) -> Dict[str, str]:
    headers = {"Accept": "application/vnd.github+json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    return headers


def otpravit_kommentarij(repo: str, pr: int, tekst: str, token: str | None) -> None:
    """Отправляет комментарий в PR или выводит его в консоль при отсутствии токена."""
    if not token:
        print("GITHUB_TOKEN не найден, вывод комментария в stdout:")
        print(tekst)
        return
    url = f"{API_BASE}/repos/{repo}/issues/{pr}/comments"
    zapros = Request(url, data=json.dumps({"body": tekst}).encode("utf-8"), headers=_get_headers(token))
    with urlopen(zapros) as response:
        print(f"Комментарий опубликован, статус: {response.status}")


def poluchit_runs(repo: str, token: str | None, limit: int = 5) -> List[Dict[str, Any]]:
    """Получает последние прогоны GitHub Actions для watchdog-отчёта."""
    url = f"{API_BASE}/repos/{repo}/actions/runs?per_page={limit}"
    zapros = Request(url, headers=_get_headers(token))
    try:
        with urlopen(zapros) as response:
            dannye = json.loads(response.read().decode("utf-8"))
    except HTTPError as oshibka:
        print(f"Не удалось получить список прогонов: {oshibka}")
        return []
    return dannye.get("workflow_runs", [])


def sobrat_watchdog_tekst(repo: str, token: str | None) -> str:
    """Формирует сводку завершённых прогонов для сторожевого сценария."""
    runs = poluchit_runs(repo, token)
    stroki = ["## Watchdog отчёт", "Последние прогоны:"]
    for run in runs:
        stroki.append(
            f"- {run.get('name','run')} #{run.get('run_number')} — {run.get('conclusion','unknown')}"
        )
    if not runs:
        stroki.append("(нет данных о прогонах)")
    return "\n".join(stroki)


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description="Kolibri PR commentator")
    parser.add_argument("--pr", type=int, help="номер PR")
    parser.add_argument("--body", help="текст комментария")
    parser.add_argument("--repository", default=os.environ.get("GITHUB_REPOSITORY", ""))
    parser.add_argument("--watchdog", action="store_true", help="сформировать отчёт без указания PR")
    args = parser.parse_args(argv)

    token = os.environ.get("GITHUB_TOKEN")

    if args.watchdog:
        if not args.repository:
            print("Не задано имя репозитория для watchdog-режима", file=sys.stderr)
            return 1
        tekst = sobrat_watchdog_tekst(args.repository, token)
        print(tekst)
        return 0

    if not args.pr or not args.body:
        print("Для публикации комментария требуются --pr и --body", file=sys.stderr)
        return 1

    if not args.repository:
        print("Неизвестный репозиторий", file=sys.stderr)
        return 1

    otpravit_kommentarij(args.repository, args.pr, args.body, token)
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main(sys.argv[1:]))
