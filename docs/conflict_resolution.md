# Разрешение конфликтов слияния / Merge Conflict Resolution / 合并冲突处理指南

## Русский

### Назначение
Этот документ описывает проверенную последовательность действий для устранения конфликтов при обновлении ветки `work`/`main` и подготовки безупречных коммитов для экосистемы «Колибри».

### Быстрый путь с `scripts/resolve_conflicts.py`
1. Обновите ссылки на удалённые ветки: `git fetch --all --prune`.
2. Запустите автоматизированный скрипт: `python scripts/resolve_conflicts.py --base origin/main --head work`.
3. Изучите сгенерированный JSON-отчёт в `build/conflict-report.json`, убедитесь, что все файлы промаркированы как `resolved`.
4. Выполните `make`, `make test`, `./kolibri.sh up` для проверки целостности и работоспособности.
5. Просмотрите дифф и зафиксируйте изменения коммитом с пометкой `[autofix conflicts]`.

### Ручной сценарий
1. Включите переиспользование резолвов: `git config rerere.enabled true`.
2. Переключитесь на рабочую ветку и подтяните основу: `git checkout work && git pull --ff-only`.
3. Попробуйте `git rebase origin/main`. При конфликте:
   - Изучите маркеры `<<<<<<<`, `=======`, `>>>>>>>`.
   - Для инфраструктуры (`Makefile`, `kolibri.sh`, workflows) отдавайте приоритет базовой ветке, внося изменения из head точечно.
   - Для ядра (`backend/src`, `apps/kolibri_node.c`) сохраните оба варианта, приведя код к стилю K&R и удостоверившись, что идентификаторы остаются на транслитерированном русском.
4. После устранения конфликтов выполните `git add`, затем `git rebase --continue` (либо `git commit` при merge-сценарии).
5. Пересоберите проект и запустите тесты. При необходимости повторите этап корректировки.

### Контрольный список перед пушем
- Чистое рабочее дерево (`git status` не содержит изменений).
- Все обязательные проверки (make, тесты, `clang-tidy`, при изменении Python — `pytest`) прошли успешно.
- Добавлена запись в `docs/рабочий_журнал.md` с описанием выполненной работы.

## English

### Purpose
This guide captures the recommended sequence for resolving merge conflicts while keeping the Kolibri repository pristine.

### Fast path with `scripts/resolve_conflicts.py`
1. Refresh remotes: `git fetch --all --prune`.
2. Run the helper: `python scripts/resolve_conflicts.py --base origin/main --head work`.
3. Inspect the generated report `build/conflict-report.json` and verify that every entry is marked `resolved`.
4. Execute `make`, `make test`, and `./kolibri.sh up` to validate the fix.
5. Review the diff and create a commit tagged `[autofix conflicts]` once satisfied.

### Manual workflow
1. Enable rerere reuse: `git config rerere.enabled true`.
2. Checkout the working branch and fast-forward: `git checkout work && git pull --ff-only`.
3. Attempt `git rebase origin/main`. On conflict:
   - Examine `<<<<<<<`, `=======`, `>>>>>>>` markers.
   - Prefer the base version for infra files (Makefile, workflows, orchestration scripts) and reapply head-specific edits intentionally.
   - For core sources (`backend/src`, `apps/kolibri_node.c`), preserve both intent variants, refactor to K&R style, and keep snake_case transliterated identifiers.
4. Stage resolved files (`git add`) and run `git rebase --continue` (or complete the merge with `git commit`).
5. Rebuild and rerun the full test suite; iterate if necessary.

### Pre-push checklist
- Working tree is clean (`git status` shows no pending changes).
- Mandatory checks (`make`, unit tests, `clang-tidy`, plus Python linters when touched) succeed.
- The work log `docs/рабочий_журнал.md` documents the resolution effort.

## 中文

### 目的
本文档记录了在保持 Kolibri 仓库整洁的前提下解决合并冲突的推荐步骤。

### 使用 `scripts/resolve_conflicts.py` 的快速流程
1. 刷新远端：`git fetch --all --prune`。
2. 运行辅助脚本：`python scripts/resolve_conflicts.py --base origin/main --head work`。
3. 查看 `build/conflict-report.json`，确认所有条目标记为 `resolved`。
4. 依次执行 `make`、`make test`、`./kolibri.sh up` 验证修复结果。
5. 检查差异并在满意后创建带有 `[autofix conflicts]` 标记的提交。

### 手工流程
1. 启用 rerere：`git config rerere.enabled true`。
2. 切换到工作分支并快进更新：`git checkout work && git pull --ff-only`。
3. 尝试 `git rebase origin/main`。若发生冲突：
   - 仔细处理 `<<<<<<<`、`=======`、`>>>>>>>` 标记。
   - 基础设施文件（Makefile、workflow、脚本）以基础分支版本为主，再有选择地融入 head 端修改。
   - 核心源码（`backend/src`、`apps/kolibri_node.c`）需保留双方意图，统一为 K&R 风格，并坚持蛇形命名的俄语音译。
4. `git add` 已解决文件，随后执行 `git rebase --continue`（或在合并场景下 `git commit`）。
5. 重新构建并运行完整测试，必要时重复修正。

### 推送前检查
- 工作区干净（`git status` 无待处理文件）。
- 必要检查（`make`、单元测试、`clang-tidy`，涉及 Python 时另加 `pytest` 等）全部通过。
- 在 `docs/рабочий_журнал.md` 中记录本次冲突处理。
