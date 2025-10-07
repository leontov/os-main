from __future__ import annotations

import argparse
import json
import sys

from .client import KolibriAgentClient


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Kolibri SDK CLI")
    parser.add_argument("--base-url", default="http://127.0.0.1:8056", help="Базовый URL Kolibri Agent API")
    subparsers = parser.add_subparsers(dest="command", required=True)

    step_parser = subparsers.add_parser("step", help="Выполнить шаг χ→Φ→S")
    step_parser.add_argument("--q", required=True, help="Входной q (число или строка)")
    step_parser.add_argument("--beam", type=int, default=16)
    step_parser.add_argument("--depth", type=int, default=8)

    state_parser = subparsers.add_parser("state", help="Снимок состояния агента")
    state_parser.add_argument("--full", action="store_true", help="Показать рабочую память")

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    args = parser.parse_args(argv)

    client = KolibriAgentClient(base_url=args.base_url)
    try:
        if args.command == "step":
            result = client.step(q=args.q, beam=args.beam, depth=args.depth)
            payload = {
                "score": result.score,
                "chi": result.chi,
                "phi": result.phi,
                "best_id": result.best_id,
                "trace": [node.__dict__ for node in result.trace[:10]],
            }
            json.dump(payload, sys.stdout, indent=2, ensure_ascii=False)
            sys.stdout.write("\n")
        elif args.command == "state":
            state = client.state()
            payload = {
                "theta": state.theta[:8],
                "pi": state.pi[:8],
                "rho": state.rho[:8],
                "sigma": state.sigma,
                "updates": state.updates,
                "ema_reward": state.ema_reward,
            }
            if args.full:
                payload["working_memory"] = state.working_memory[:5]
            json.dump(payload, sys.stdout, indent=2, ensure_ascii=False)
            sys.stdout.write("\n")
    finally:
        client.close()
    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
