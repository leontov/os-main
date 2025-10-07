```kolibri-policy
build: ours
code: ours
docs: ours

files:
  prefer_ours:
    - backend/**
    - frontend/**
    - scripts/**
  prefer_theirs:
    - docs/**
    - README.md

budgets:
  wasm_max_kb: 6144
  step_latency_ms: 250
  coverage_min_lines: 75
  coverage_min_branches: 60
```
