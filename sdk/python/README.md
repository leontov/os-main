# Kolibri SDK (Python)

Kolibri SDK предоставляет Python-интерфейс к автономному ядру Kolibri Nano:

- HTTP-клиент для `/api/agent/step` и `/api/agent/state`.
- Типы данных для трассировки χ→Φ→S и просмотра рабочей памяти.
- CLI `kolibri-cli` для быстрой проверки шага и состояния агента.

## Установка

```bash
pip install kolibri-sdk-0.5.0-py3-none-any.whl
```

(Создаётся с помощью `python -m build` в каталоге `sdk/python`).

## Пример использования

```python
from kolibri_sdk import KolibriAgentClient

client = KolibriAgentClient(base_url="http://127.0.0.1:8056")

step = client.step(q=42, beam=12, depth=6)
print(step.score, step.trace[:3])

state = client.state()
print(state.updates, state.theta[:4])
```

## CLI

```bash
kolibri-cli step --q 42 --beam 12 --depth 6
kolibri-cli state
```

Для полноценной работы требуется запущенный Kolibri agent API (`uvicorn backend.agent.main:app`).
