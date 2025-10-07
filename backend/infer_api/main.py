
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, conint, conlist
import os, subprocess
from pathlib import Path

BIN = os.getenv("KNP_INFER_BIN", "apps/kolibri_infer")
app = FastAPI(title="Kolibri Nano Infer API", version="0.2.0")


def _load_theta_from_file(path: str) -> list[float]:
    if not path:
        return []
    file_path = Path(path)
    if not file_path.is_absolute():
        file_path = (Path.cwd() / file_path).resolve()
    try:
        content = file_path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return []

    values: list[float] = []
    for chunk in content.replace("\n", ",").split(","):
        token = chunk.strip()
        if not token:
            continue
        try:
            values.append(float(token))
        except ValueError:
            return []
    return values

class InferRequest(BaseModel):
    q: conint(ge=1)
    beam: conint(ge=1, le=256) = 8
    depth: conint(ge=1, le=128) = 6
    theta: conlist(float, min_items=0, max_items=32) | None = None

class InferResponse(BaseModel):
    best_id: int
    value: float
    score: float

@app.post("/api/infer", response_model=InferResponse)
def infer(req: InferRequest):
    args = [BIN, "--q", str(req.q), "--beam", str(req.beam), "--depth", str(req.depth)]
    env = os.environ.copy()
    theta_values = list(req.theta) if req.theta else None
    theta_file = env.get("KNP_THETA_FILE", "data/knp_theta.csv")
    resolved_theta_file = str((Path.cwd() / theta_file).resolve()) if not Path(theta_file).is_absolute() else theta_file
    env["KNP_THETA_FILE"] = resolved_theta_file
    if theta_values is None:
        file_theta = _load_theta_from_file(resolved_theta_file)
        if file_theta:
            theta_values = file_theta
    if theta_values:
        env["KNP_THETA"] = ",".join(str(x) for x in theta_values)
    try:
        p = subprocess.run(args, env=env, capture_output=True, text=True, timeout=5)
        if p.returncode != 0:
            raise HTTPException(500, f"infer failed: {p.stderr.strip() or p.stdout.strip()}")
        parts = p.stdout.strip().split()
        if len(parts)!=3:
            raise HTTPException(500, f"bad output: {p.stdout!r}")
        return InferResponse(best_id=int(parts[0]), value=float(parts[1]), score=float(parts[2]))
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(500, f"error: {e}")
