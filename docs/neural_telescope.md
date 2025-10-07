# Kolibri Neural Telescope / Нейронный телескоп Kolibri / Kolibri 神经望远镜

**Copyright (c) 2025 Кочуров Владислав Евгеньевич**

---

## 1. Vision / Видение / 愿景

- **EN:** The Neural Telescope is a WebGPU-powered observation layer that renders the live Kolibri swarm without resorting to synthetic simulations. Every particle corresponds to a real node streaming verified deltas from the production network.
- **RU:** «Нейронный телескоп» — это слой наблюдения на базе WebGPU, отображающий живой рой Kolibri без каких-либо симуляций. Каждая частица на экране соответствует реальному узлу, передающему в UI проверенные дельты из рабочей сети.
- **ZH:** “神经望远镜” 是基于 WebGPU 的观测层，用于呈现真实运行的 Kolibri 群体，而非任何形式的模拟。画面中的每个粒子都对应一台真实节点，并通过生产网络发送经过验证的增量数据。

---

## 2. Kolibri Visualization Protocol (KVP) / Протокол визуализации Kolibri / Kolibri 可视化协议

- **EN:** Aggregator nodes collect state changes (node activity, genome transfers, health) from clusters of ~1000 peers, compress them into delta frames, and deliver them via persistent WebSockets. Frames include HMAC-verified headers, timestamps, and compact payloads for direct GPU upload.
- **RU:** Узлы-агрегаторы собирают изменения состояния (активность узла, передачи генома, состояние здоровья) с кластеров ≈1000 соседей, сжимают их в дельта-кадры и передают по постоянным WebSocket-соединениям. Кадры содержат заголовки с HMAC-подписью, временные метки и компактную полезную нагрузку для прямой загрузки в GPU.
- **ZH:** 汇聚节点从约 1000 个邻居收集状态变化（节点活跃度、基因传输、健康状况），压缩成增量帧并通过持久化 WebSocket 发送。帧内包含 HMAC 校验头、时间戳以及可直接上传至 GPU 的精简负载。

---

## 3. WebGPU Scene Graph / Сценограф WebGPU / WebGPU 场景图

- **EN:** The frontend maintains GPU-side buffers: `positionsBuffer` for static layout, `stateBuffer` for dynamic attributes (color, scale, pulse). Incoming KVP deltas trigger partial buffer updates using `queue.writeBuffer` without redrawing the entire swarm. Vertex and fragment shaders (`swarm_vertex.wgsl`, `swarm_fragment.wgsl`) map raw states to animated visuals.
- **RU:** На фронтенде поддерживаются буферы на GPU: `positionsBuffer` для статической раскладки, `stateBuffer` для динамических атрибутов (цвет, масштаб, пульс). Входящие дельты KVP вызывают частичные обновления через `queue.writeBuffer`, что исключает полную перерисовку роя. Вершинный и фрагментный шейдеры (`swarm_vertex.wgsl`, `swarm_fragment.wgsl`) преобразуют состояния в анимированное представление.
- **ZH:** 前端在 GPU 上维护 `positionsBuffer`（静态布局）与 `stateBuffer`（动态属性：颜色、缩放、脉动）。收到 KVP 增量后，通过 `queue.writeBuffer` 执行局部更新，无需整体重绘。顶点与片元着色器（`swarm_vertex.wgsl`, `swarm_fragment.wgsl`）负责将原始状态映射为动态可视化。

---

## 4. Compute Acceleration / Вычислительное ускорение / 计算加速

- **EN:** A dedicated compute pipeline (`vote_aggregator.wgsl`) accelerates fractal vote aggregation. The WASM core exports `uskorit_golosovanie_cherez_gpu`, which marshals digit streams into storage buffers, dispatches the compute pass, and retrieves aggregated votes with predictable latency.
- **RU:** Отдельный вычислительный конвейер (`vote_aggregator.wgsl`) ускоряет агрегацию голосов фрактальной памяти. WASM-ядро экспортирует функцию `uskorit_golosovanie_cherez_gpu`, которая упаковывает цифровые потоки в storage-буферы, запускает вычислительный проход и возвращает агрегированные голоса с предсказуемой задержкой.
- **ZH:** 专用计算管线 (`vote_aggregator.wgsl`) 用于加速分形记忆的投票归并。WASM 内核导出 `uskorit_golosovanie_cherez_gpu`，负责将数字流写入 storage 缓冲区、调度计算着色器并以可预测延迟取回聚合结果。

---

## 5. Milestones / Контрольные точки / 里程碑

1. **Telemetry Bridge / Телемост / 遥测桥** — Реализация KVP, аггрегаторов и подписки UI. / Implement KVP, aggregators, and UI subscription.
2. **GPU Dashboard / GPU-пульт / GPU 仪表盘** — Запуск WebGPU-сцены с ≥1 млн частиц при 60 FPS. / Render ≥1M particles at 60 FPS.
3. **Compute Lift / Вычислительный прорыв / 计算突破** — Связка WASM ↔ WebGPU, ускоряющая агрегацию ≥10×. / Achieve ≥10× faster aggregation via compute shaders.
4. **Interactive Analysis / Интерактивный анализ / 交互分析** — Фильтры, зум, поиск событий без потери потока. / Add filters, zoom, and event search without interrupting the stream.

---

## 6. References / Ссылки / 参考

- `docs/web_interface.md` — фронтенд и PWA. / frontend & PWA.
- `docs/architecture.md` — общая архитектура. / overall architecture.
- `docs/kolibri_integrated_prototype.md` — интегрированный прототип. / integrated prototype.

