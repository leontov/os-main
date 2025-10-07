# Kolibri Architecture / Архитектура Kolibri / Kolibri 架构

**Copyright (c) 2025 Кочуров Владислав Евгеньевич**

---

## 1. Overview / Обзор / 概览

### Русский
Kolibri — это модульная система, в которой каждая подсистема реализована в чистом C и связана предсказуемыми интерфейсами. Ключевые слои:
1. **Decimal Cognition** (`backend/src/decimal.c`) — преобразует внешние данные в десятичные импульсы и обратно.
2. **Formula Evolution** (`backend/src/formula.c`) — эволюционный пул формул, управляющий «геномом» знаний.
3. **Digital Genome** (`backend/src/genome.c`) — криптографический журнал ReasonBlock, фиксирующий события и формулы.
4. **Swarm Networking** (`backend/src/net.c`) — бинарный протокол для обмена лучшими формулами между узлами.
5. **Kolibri Node CLI** (`apps/kolibri_node.c`) — оболочка, которая объединяет все подсистемы и предоставляет REPL/daemon режим.
6. **Тесты** (`tests/`) — регрессионный каркас, обеспечивающий воспроизводимость.

### English
Kolibri is a modular system implemented in pure C with predictable boundaries between subsystems. The major layers are identical to the list above with focus on deterministic APIs and minimal dependencies.

### 中文
Kolibri 采用纯 C 模块化实现，子系统之间通过稳定接口协作。主要层级如上所列，强调确定性与最小依赖。

---

## 2. Component Responsibilities / Ответственность компонентов / 组件职责

### Decimal Cognition
- **API:** `k_encode_text`, `k_decode_text`, и функции оценки длины буферов.
- **Назначение:** конвертация входа/выхода в цифры `0–9`, обеспечение обратимости.
- **Артефакты:** `backend/include/kolibri/decimal.h`, `backend/src/decimal.c`, тест `tests/test_decimal.c`.

### Formula Evolution
- **API:** `kf_pool_init`, `kf_pool_tick`, `kf_pool_best`, `kf_formula_apply`.
- **Механика:** ограниченный пул из 16 формул с мутациями коэффициентов `a`, `b`; fitness пересчитывается при каждом `tick`.
- **Артефакты:** `backend/include/kolibri/formula.h`, `backend/src/formula.c`, тесты `tests/test_formula.c`.

### Digital Genome
- **API:** `kg_open`, `kg_append`, `kg_close`.
- **Особенности:** HMAC-SHA256 для подписи, хранение хэшей цепочки, максимальные размеры событий.
- **Артефакты:** `backend/include/kolibri/genome.h`, `backend/src/genome.c`, тест `tests/test_genome.c`.

### Swarm Networking
- **API:** `kn_message_encode_*`, `kn_message_decode`, `kn_listener_*`, `kn_share_formula`.
- **Назначение:** сериализация сообщений HELLO/MIGRATE_RULE/ACK, TCP-соединения для миграции формул.
- **Артефакты:** `backend/include/kolibri/net.h`, `backend/src/net.c`, тест `tests/test_net.c`.

### Application Layer
- **Колибри-узел:** аргументы командной строки (`--seed`, `--node-id`, `--listen`, `--peer`), REPL-команды (`:good`, `:bad`, `:why`, `:canvas`, `:sync`, `:verify`).
- **Скрипты:** `kolibri.sh` автоматизирует сборку, запуск тестов, старт кластера.

---

## 3. Data Flow / Потоки данных / 数据流

1. Пользовательский ввод поступает в `apps/kolibri_node` → кодируется функцией `k_encode_text` → формирует импульсы для формульного пула.
2. `KolibriFormulaPool` обновляет формулы, вычисляя fitness, и выбирает лучшую формулу для текущего контекста.
3. События обучения записываются в `KolibriGenome` как `ReasonBlock` с HMAC.
4. При конфигурации `--peer` лучшая формула пакуется `kn_message_encode_formula` и отправляется через TCP.
5. Полученный пакет декодируется `kn_message_decode`, формула интегрируется и событие логируется.

---

## 4. Determinism & Reproducibility / Детерминизм и воспроизводимость / 确定性与可复现性

- **RNG:** `KolibriRng` использует линейный конгруэнтный генератор, инициализируемый `--seed` (или значением по умолчанию).
- **Формулы:** любые мутации зависят только от RNG, входного набора и текущей популяции.
- **Геном:** каждый блок включает индекс, метку времени, хеш предыдущего блока и HMAC, исключая расхождения между узлами.
- **Тесты:** `make test` проверяет функциональность каждого слоя, а `clang-tidy` обеспечивает статический анализ.

---

## 5. Deployment Targets / Целевые окружения / 部署目标

- **Native:** сборка `make` производит бинарники в `build/`.
- **Cluster:** `kolibri.sh up` запускает многопроцессный рой для локального тестирования.
- **WASM (план):** ядро поддерживает компиляцию в WebAssembly через Emscripten (см. `web_interface.md`).
- **Kolibri OS:** минимальная оболочка загружается через `kolibri_os.md`.

---

## 6. Extensibility / Расширяемость / 可扩展性

- Новые типы сообщений должны расширять `KolibriNetMessageType` и сопровождаться тестами.
- Дополнительные формульные операторы могут быть добавлены в `KolibriFormula` при сохранении совместимости сериализации.
- Подключение плагинов осуществляется через документированный API (см. `developer_guide.md`).

