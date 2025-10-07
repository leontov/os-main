Federation
==========

Kolibri Nano поддерживает оператор `ΔΘ` для обмена параметрами без передачи пользовательских данных.

Endpoints
---------

* ``POST /api/federation/export`` — возвращает подписанную дельту с DP-шумом (настраивается `KOLIBRI_DP_NOISE`).
* ``POST /api/federation/merge`` — принимает подписанную дельту и агрегирует с локальным состоянием.

Подписи и шум
-------------

* HMAC ключ задаётся ``KOLIBRI_FEDERATION_KEY``.
* Для защиты от реконструкции используется равномерный шум (опционально).
* Агрегирование производит взвешенное среднее по ``updates``.

Pseudocode
---------

.. code-block:: python

   delta = ThetaDelta.from_state(local_state, noise_scale=0.05)
   payload = sign_delta(delta, key)
   remote_delta = verify_and_load(payload, key)
   merged = merge_deltas(local_state, [remote_delta])
