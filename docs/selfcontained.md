# Самодостаточный Kolibri OS — инструкция

В этом репозитории собран минимальный самодостаточный пакет Kolibri, содержащий нативный бинарник, WebAssembly‑ядро (минимальная сборка) и фронтенд.

Артефакты
- `dist/kolibri_selfcontained.tar.gz` — самодостаточный tarball с:
  - `kolibri_node` — нативный бинарник (build/kolibri_node)
  - `libkolibri_core.a` — статическая библиотека
  - `wasm/kolibri.wasm` — WebAssembly (минимальная сборка; цифровой геном может быть отключён)
  - `frontend/dist/*` — собранный фронтенд (production)

Быстрый запуск (локальный):

```bash
./scripts/run_local.sh
```

Это распакует архив во временную директорию, попытается запустить нативный бинарник (если он есть) и поднимет статический HTTP сервер для фронтенда на `http://localhost:8000`.

Полноценная сборка WASM с цифровым геномом

Чтобы собрать `kolibri.wasm` с поддержкой цифрового генома (HMAC), требуется окружение Emscripten с доступом к заголовкам OpenSSL, или запуск сборки внутри Docker-образа emscripten.

Варианты:

- Собрать через Docker (рекомендуется, воспроизводимо):

  ```bash
  # нужно установить Docker
  ./scripts/build_wasm.sh
  ```

  Скрипт автоматически вызовет docker image `emscripten/emsdk:3.1.61`, если `emcc` не обнаружен.

- Локальная сборка через emcc (если установлен Emscripten):

  ```bash
  brew install openssl@3
  export CFLAGS="-I/opt/homebrew/opt/openssl@3/include"
  export LDFLAGS="-L/opt/homebrew/opt/openssl@3/lib"
  export KOLIBRI_WASM_INCLUDE_GENOME=1
  ./scripts/build_wasm.sh
  ```

Замечание: при отсутствии OpenSSL в окружении emcc сборка завершается ошибкой `openssl/hmac.h not found`.

Документация и релизы

Добавьте `dist/kolibri_selfcontained.tar.gz` в релизный артефакт или публикуйте образ Docker с включёнными бинарниками для полной самодостаточности.
