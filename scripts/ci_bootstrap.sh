#!/usr/bin/env bash
set -euo pipefail

PYTHON_BIN="${PYTHON:-python3}"

$PYTHON_BIN -m pip install --upgrade pip
$PYTHON_BIN -m pip install pytest ruff pyright coverage fastapi pydantic asyncpg clickhouse-connect

echo "Инструменты установлены: pytest, ruff, pyright, coverage"
