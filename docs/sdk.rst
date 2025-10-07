SDK Usage
=========

Python
------

.. code-block:: bash

   pip install kolibri-sdk-0.5.0-py3-none-any.whl

.. code-block:: python

   from kolibri_sdk import KolibriAgentClient

   client = KolibriAgentClient(base_url="http://127.0.0.1:8056")
   step = client.step(q=42, beam=12, depth=6)
   print(step.score, step.trace[:2])

CLI
---

.. code-block:: bash

   kolibri-cli step --q 42 --beam 12 --depth 6
   kolibri-cli state --full

JavaScript
----------

.. code-block:: bash

   npm install @kolibri/sdk

.. code-block:: typescript

   import { KolibriAgent } from "@kolibri/sdk";

   const agent = new KolibriAgent("http://127.0.0.1:8056");
   const step = await agent.step({ q: 42, beam: 12, depth: 6 });
