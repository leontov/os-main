# Web Interface & WASM Bridge / Веб-интерфейс и WASM-мост / Web 界面与 WASM 桥接

**Copyright (c) 2025 Кочуров Владислав Евгеньевич**

---

## 1. Goals / Цели / 目标

- Предоставить визуализацию фрактальной памяти и формул.
- Поддержать офлайн-режим (PWA) и запуск ядра в браузере.
- Обеспечить обмен данными между React-компонентами и WebAssembly.

---

## 2. Project Layout / Структура проекта / 项目结构

```
frontend/
  ├─ src/
  │   ├─ App.tsx
  │   ├─ components/
  │   │    ├─ Chat.tsx
  │   │    ├─ NodeGraph.tsx
  │   │    ├─ FractalMemory.tsx
  │   │    ├─ GenomeExplorer.tsx
  │   │    ├─ RuleEditor.tsx
  │   │    └─ BrainAnalytics.tsx
  │   └─ wasm/
  │        └─ kolibri.ts
  └─ vite.config.ts
```

---

## 3. WASM Compilation / Компиляция WASM / WASM 编译

1. Установите Emscripten (`emsdk`).
2. Выполните: `emcmake cmake -S . -B build-wasm`.
3. `emmake cmake --build build-wasm --target kolibri_core`.
4. Результат: `build-wasm/kolibri.wasm` + glue JS.

---

## 4. JS Bridge / JavaScript-мост / JavaScript 桥接

```ts
import ModuleFactory from "./kolibri.wasm";

export async function createKolibriCore() {
  const module = await ModuleFactory();
  return {
    encode: (text: string) => module._k_encode_text(text),
    tick: (inputPtr: number, len: number) => module._kf_pool_tick(inputPtr, len),
    getBest: () => module._kf_pool_best(),
  };
}
```

- Все указатели управляются через `Module._malloc`/`_free`.
- Объект core передаётся в React context.

---

## 5. PWA Features / Возможности PWA / PWA 特性

- Service Worker кэширует `index.html`, `kolibri.wasm`, статические ассеты.
- Manifest описывает иконки, режим standalone, цветовую схему.
- При отсутствии сети UI переключается в offline-маршрут с локальной визуализацией генома.

---

## 6. Visualization Components / Компоненты визуализации / 可视化组件

| Component | Назначение |
|-----------|-----------|
| `Chat` | Диалог с ядром и команды REPL. |
| `NodeGraph` | Топология роя и задержки между узлами. |
| `FractalMemory` | Дерево десятичных уровней с подсветкой активных путей. |
| `GenomeExplorer` | Просмотр цепочки ReasonBlock. |
| `RuleEditor` | Ручная настройка формул, загрузка/выгрузка в пул. |
| `BrainAnalytics` | Графики fitness, мутаций и эффективности сети. |

---

## 7. Deployment / Развёртывание / 部署

- `npm install` → `npm run build` (создаёт `dist/`).
- Разместите `dist/` на статическом хостинге (GitHub Pages, Netlify).
- Для локальной отладки используйте `npm run dev` с проксированием к локальному Kolibri Node.

---

## 8. Security Considerations / Безопасность / 安全注意事项

- WebAssembly модуль не должен иметь сетевых побочных эффектов без явного разрешения пользователя.
- Логи генома сохраняются в IndexedDB с шифрованием (планируется).
- Все команды пользователя отображаются в журнале с отметкой времени.

