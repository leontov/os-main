# Release v0.1.0 — Developer Preview

Release date: 2025-10-11

Summary
-------
Developer preview release of Kolibri: a self-contained local-first research OS with a small inference scaffold, agent persistence, and swarm utilities for exchanging learned formulas.

Artifacts
---------
- dist/kolibri_selfcontained.tar.gz
- dist/kolibri_selfcontained.tar.gz.sha256
- (CI) build/wasm/kolibri.wasm — full wasm including genome/HMAC (attached by CI when available)

Highlights
----------
- Local agent framework with persistence and runtime hooks.
- Rule-based inference pipeline and formula evolution engine.
- Web desktop prototype with Terminal app and wasm bridge (stubbed when full wasm is not present).
- CI workflow that builds a full wasm artifact inside an emscripten Docker image and packages release assets.

Notes for users
--------------
- If the full wasm artifact is not attached, the frontend will run in stub mode and some features tied to the digital genome will be disabled. The CI workflow is configured to build and attach the full wasm; please wait for the workflow run to complete.

Checksums
---------
See `dist/kolibri_selfcontained.tar.gz.sha256` for the SHA256 checksum of the packaged demo.

How to reproduce
----------------
See `.github/workflows/build_release.yml` and `scripts/build_wasm.sh` for the CI build steps. Local reproducible builds are supported inside an emscripten Docker image (recommended).

Contributors
------------
- Development team and contributors (see git history for full list).
