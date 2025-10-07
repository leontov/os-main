# Kolibri Digital Genome / Цифровой геном Kolibri / Kolibri 数字基因组

**Copyright (c) 2025 Кочуров Владислав Евгеньевич**

---

## 1. Purpose / Назначение / 目的

Хранить неизменяемый журнал событий Kolibri с криптографическими гарантиями целостности и происхождения.

---

## 2. Structure of ReasonBlock / Структура ReasonBlock / ReasonBlock 结构

| Field | Size | Description |
|-------|------|-------------|
| `index` | 8 байт | Последовательный номер блока. |
| `timestamp` | 8 байт | UNIX-время в наносекундах. |
| `prev_hash` | 32 байта | SHA-256 предыдущего блока. |
| `hmac` | 32 байта | HMAC-SHA256 текущего блока. |
| `event_type` | 32 байта | Нуль-терминированная строка (UTF-8). |
| `payload` | 256 байт | Данные события (десятичные строки). |

---

## 3. API / Программный интерфейс / API

- `int kg_open(KolibriGenome *ctx, const char *path, const unsigned char *key, size_t key_len);`
  - Создаёт файл `genome.dat` (если отсутствует) и инициализирует HMAC-ключ.
- `int kg_append(KolibriGenome *ctx, const char *event_type, const char *payload, ReasonBlock *out_block);`
  - Формирует новый блок, вычисляет хэши и дописывает в конец файла.
- `void kg_close(KolibriGenome *ctx);`
  - Закрывает файл и очищает структуру.
- `int kg_verify_file(const char *path, const unsigned char *key, size_t key_len);`
  - **RU:** Возвращает `0` при успешной проверке журнала, `1` если файл не найден,
    `-1` при нарушении целостности или ошибке чтения.
  - **EN:** Returns `0` on success, `1` when the ledger file is missing, and `-1`
    when integrity validation fails.
  - **ZH:** 验证成功返回 `0`，文件不存在返回 `1`，若校验失败则返回 `-1`。

---

## 4. HMAC Calculation / Расчёт HMAC / HMAC 计算

1. Собирается бинарное представление блока без поля `hmac`.
2. Вычисляется `HMAC_SHA256(key, data)`.
3. Результат записывается в `hmac`.
4. Все байты хранятся в big-endian порядке.

---

## 5. File Layout / Макет файла / 文件结构

```
+-----------+-----------+-----------+-----+
| Block 0   | Block 1   | Block 2   | ... |
+-----------+-----------+-----------+-----+
```

Каждый блок имеет фиксированную длину 8+8+32+32+32+256 = 368 байт.

---

## 6. Usage Patterns / Типовые сценарии / 使用模式

- Запись успешной формулы: `event_type="FORMULA_EVOLVED"`, `payload` содержит параметры и fitness.
- Верификация синхронизации: узлы роя обмениваются последним `index` и `prev_hash`.
- Журналирование пользовательских действий: `event_type="USER_FEEDBACK"`, `payload` хранит оценку `:good`/`:bad`.

---

## 7. Verification Workflow / Проверка целостности / 校验流程

1. Считайте файл целиком.
2. Для каждого блока пересчитайте HMAC и сравните с записанным значением.
3. Проверьте, что `prev_hash` блока `n` совпадает с SHA-256 блока `n-1`.
4. Любое расхождение → журнал считается повреждённым.

> **CLI Shortcut / Быстрый запуск / 命令行快捷方式:**
> `./build/kolibri_node --verify-genome [--genome <path>]` выполняет описанную
> проверку перед запуском узла и прекращает работу при любой ошибке.

---

## 8. Operational Guidelines / Операционные указания / 运维指南

- Храните HMAC-ключ отдельно от журнала.
- Резервное копирование выполняется поблочно с проверкой хэшей.
- Никогда не редактируйте файл вручную — используйте только `kg_append`.

