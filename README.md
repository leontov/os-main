# Kolibri OS

Колибри OS — легковесная экспериментальная платформа, объединяющая KolibriScript, симулятор и набор утилит для отладки цифровых сценариев. Этот документ описывает, как развернуть окружение разработчика и запустить основные проверки.

## Требования
- Python 3.10+
- `pip` и `virtualenv`
- Компилятор C/C++ с поддержкой CMake 3.20+
- Ninja либо Make (по желанию)

## Быстрый старт
1. Клонируйте репозиторий и перейдите в директорию проекта.
2. Подготовьте виртуальное окружение Python (поддерживаются версии 3.10+):
   ```bash
   python -m venv .venv
   source .venv/bin/activate  # Windows: .venv\\Scripts\\activate
   python -m pip install --upgrade pip
   ```
3. Установите инструменты, перечисленные в [`requirements.txt`](requirements.txt). Файл включает точные версии `pytest`, `coverage`, `ruff` и `pyright`, которые используются в CI.
   ```bash
   pip install -r requirements.txt
   ```
4. Соберите C-компоненты Kolibri:
   ```bash
   cmake -S . -B build -G "Ninja"  # или опустите -G, чтобы использовать Makefiles
   cmake --build build
   ```

5. Для веб-интерфейса соберите wasm-ядро перед фронтендом:
   ```bash
   ./scripts/build_wasm.sh
   ```
6. Соберите фронтенд (после установки npm-зависимостей в `frontend/`):
   ```bash
   cd frontend
   npm install
   npm run build
   ```
7. Запустите тесты:

   ```bash
   pytest -q
   ruff check .
   pyright
   ctest --test-dir build
   ```

   ## Релизы и CI

   Релизы собираются автоматически при push тега (workflow `.github/workflows/build_release.yml`). CI собирает `kolibri.wasm` внутри Docker (emscripten image) и пакует релиз.

   Artifacts:
   - `dist/release/kolibri_release_<tag>.tar.gz` — релизный tarball с нативным бинарником, wasm и фронтендом.
   - `dist/kolibri_selfcontained.tar.gz` — локальный самодостаточный пакет для тестов.

## Проверки качества
- Линтеры Python: `ruff check`, `pyright`
- Политики проекта: `python scripts/policy_validate.py`
- Форматирование C-кода выполняется стандартными средствами компилятора; следуйте существующему стилю файлов в `apps/` и `tests/`.

## Дополнительные ресурсы
- [План релиза](docs/project_plan.md) описывает долгосрочные вехи и критерии готовности.
- Скрипты и утилиты размещены в `scripts/`; каждый скрипт содержит встроенные подсказки по использованию.

