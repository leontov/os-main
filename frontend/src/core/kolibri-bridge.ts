/**
 * kolibri-bridge.ts
 *
 * WebAssembly-backed bridge that executes KolibriScript programs inside the
 * browser. The bridge loads `kolibri.wasm`, initialises the Kolibri runtime
 * exported by the module, and exposes a single `ask` method used by the UI.
 */
import { createWasiContext } from "./wasi";

export interface KolibriBridge {
  readonly ready: Promise<void>;
  ask(prompt: string, mode?: string): Promise<string>;
  reset(): Promise<void>;
}

interface KolibriWasmExports extends WebAssembly.Exports {
  memory: WebAssembly.Memory;
  _malloc(size: number): number;
  _free(ptr: number): void;
  _kolibri_bridge_init(): number;
  _kolibri_bridge_reset(): number;
  _kolibri_bridge_execute(programPtr: number, outputPtr: number, outputCapacity: number): number;
}

const OUTPUT_CAPACITY = 8192;
const DEFAULT_MODE = "Быстрый ответ";
const WASM_RESOURCE_URL = "/kolibri.wasm";
const WASM_INFO_URL = "/kolibri.wasm.txt";

const COMMAND_PATTERN = /^(показать|обучить|спросить|тикнуть|сохранить)/i;
const PROGRAM_START_PATTERN = /начало\s*:/i;
const PROGRAM_END_PATTERN = /конец\./i;

function escapeScriptString(value: string): string {
  return value.replace(/\\/g, "\\\\").replace(/"/g, '\\"');
}

function normaliseLines(input: string): string[] {
  return input
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter((line) => line.length > 0);
}

function buildScript(prompt: string, mode: string): string {
  const trimmed = prompt.trim();
  if (!trimmed) {
    return `начало:\n    показать "Пустой запрос"\nконец.\n`;
  }

  if (PROGRAM_START_PATTERN.test(trimmed) && PROGRAM_END_PATTERN.test(trimmed)) {
    return trimmed.endsWith("\n") ? trimmed : `${trimmed}\n`;
  }

  const lines = normaliseLines(trimmed);
  const modeLine = mode && mode !== DEFAULT_MODE ? `    показать "Режим: ${escapeScriptString(mode)}"\n` : "";

  const scriptLines = lines.map((line) => {
    if (COMMAND_PATTERN.test(line)) {
      return `    ${line}`;
    }
    return `    показать "${escapeScriptString(line)}"`;
  });

  return `начало:\n${modeLine}${scriptLines.join("\n")}\nконец.\n`;
}

async function describeWasmFailure(error: unknown): Promise<string> {
  const baseReason =
    error instanceof Error && error.message ? error.message : String(error ?? "Неизвестная ошибка");

  try {
    const response = await fetch(WASM_INFO_URL);
    if (!response.ok) {
      return baseReason;
    }

    const infoText = (await response.text()).trim();
    if (!infoText) {
      return baseReason;
    }

    return `${baseReason}\n\n${infoText}`;
  } catch (infoError) {
    console.debug("[kolibri-bridge] Не удалось получить информацию о kolibri.wasm.", infoError);
    return baseReason;
  }
}

class KolibriWasmBridge implements KolibriBridge {
  private readonly encoder = new TextEncoder();
  private readonly decoder = new TextDecoder("utf-8");
  private exports: KolibriWasmExports | null = null;
  readonly ready: Promise<void>;

  constructor() {
    this.ready = this.initialise();
  }

  private async instantiateWasm(): Promise<WebAssembly.Instance> {
    const wasi = createWasiContext((text) => {
      console.debug("[kolibri-bridge][wasi]", text);
    });
    const importObject: WebAssembly.Imports = {
      wasi_snapshot_preview1: wasi.imports,
    };

    if ("instantiateStreaming" in WebAssembly) {
      try {
        const streamingResult = await WebAssembly.instantiateStreaming(fetch(WASM_RESOURCE_URL), importObject);
        const instance = streamingResult.instance;
        const exports = instance.exports as KolibriWasmExports;
        if (exports.memory instanceof WebAssembly.Memory) {
          wasi.setMemory(exports.memory);
        }
        return instance;
      } catch (error) {
        // Fallback to ArrayBuffer path when MIME type is missing.
        console.warn("Kolibri WASM streaming instantiation failed, retrying with ArrayBuffer.", error);
      }
    }

    const response = await fetch(WASM_RESOURCE_URL);
    if (!response.ok) {
      throw new Error(`Не удалось загрузить kolibri.wasm: ${response.status} ${response.statusText}`);
    }
    const bytes = await response.arrayBuffer();
    const { instance } = await WebAssembly.instantiate(bytes, importObject);
    const exports = instance.exports as KolibriWasmExports;
    if (exports.memory instanceof WebAssembly.Memory) {
      wasi.setMemory(exports.memory);
    }
    return instance;
  }

  private async initialise(): Promise<void> {
    const instance = await this.instantiateWasm();
    const exports = instance.exports as KolibriWasmExports;
    if (typeof exports._kolibri_bridge_init !== "function") {
      throw new Error("WASM-модуль не содержит kolibri_bridge_init");
    }
    const result = exports._kolibri_bridge_init();
    if (result !== 0) {
      throw new Error(`Не удалось инициализировать KolibriScript (код ${result})`);
    }
    this.exports = exports;
  }

  async ask(prompt: string, mode: string = DEFAULT_MODE): Promise<string> {
    await this.ready;
    if (!this.exports) {
      throw new Error("Kolibri WASM мост не готов");
    }

    const exports = this.exports;
    const script = buildScript(prompt, mode);
    const scriptBytes = this.encoder.encode(script);
    const programPtr = exports._malloc(scriptBytes.length + 1);
    const outputPtr = exports._malloc(OUTPUT_CAPACITY);

    if (!programPtr || !outputPtr) {
      if (programPtr) {
        exports._free(programPtr);
      }
      if (outputPtr) {
        exports._free(outputPtr);
      }
      throw new Error("Недостаточно памяти для выполнения KolibriScript");
    }

    try {
      const heap = new Uint8Array(exports.memory.buffer);
      heap.set(scriptBytes, programPtr);
      heap[programPtr + scriptBytes.length] = 0;

      const written = exports._kolibri_bridge_execute(programPtr, outputPtr, OUTPUT_CAPACITY);
      if (written < 0) {
        throw new Error(this.describeExecutionError(written));
      }

      const outputBytes = heap.subarray(outputPtr, outputPtr + written);
      const text = this.decoder.decode(outputBytes);
      return text.trim().length === 0 ? "KolibriScript завершил работу без вывода." : text.trimEnd();
    } finally {
      exports._free(programPtr);
      exports._free(outputPtr);
    }
  }

  async reset(): Promise<void> {
    await this.ready;
    if (!this.exports) {
      throw new Error("Kolibri WASM мост не готов");
    }

    const result = this.exports._kolibri_bridge_reset();
    if (result !== 0) {
      throw new Error(`Не удалось сбросить KolibriScript (код ${result})`);
    }
  }

  private describeExecutionError(code: number): string {
    switch (code) {
      case -1:
        return "Не удалось инициализировать KolibriScript.";
      case -2:
        return "WASM-модуль не смог подготовить временный вывод.";
      case -3:
        return "KolibriScript сообщил об ошибке при разборе программы.";
      case -4:
        return "Во время выполнения KolibriScript произошла ошибка.";
      case -5:
        return "Некорректные аргументы вызова KolibriScript.";
      default:
        return `Неизвестная ошибка KolibriScript (код ${code}).`;
    }
  }
}

class KolibriFallbackBridge implements KolibriBridge {
  readonly ready = Promise.resolve();
  private readonly reason: string;

  constructor(error: unknown) {
    if (error instanceof Error && error.message) {
      this.reason = error.message;
    } else {
      this.reason = String(error ?? "Неизвестная ошибка");
    }
  }

  async ask(_prompt: string, _mode?: string): Promise<string> {
    void _prompt;
    void _mode;
    return [
      "KolibriScript недоступен: kolibri.wasm не был загружен.",
      `Причина: ${this.reason}`,
      "Запустите scripts/build_wasm.sh и перезапустите фронтенд, чтобы восстановить работоспособность ядра.",
    ].join("\n");
  }

  async reset(): Promise<void> {
    // Нет состояния для сброса в режим без WASM.
  }
}

const createBridge = async (): Promise<KolibriBridge> => {
  const wasmBridge = new KolibriWasmBridge();

  try {
    await wasmBridge.ready;
    return wasmBridge;
  } catch (error) {
    console.warn("[kolibri-bridge] Переход в деградированный режим без WebAssembly.", error);
    const reason = await describeWasmFailure(error);
    return new KolibriFallbackBridge(reason);
  }
};

const bridgePromise: Promise<KolibriBridge> = createBridge();

const kolibriBridge: KolibriBridge = {
  ready: bridgePromise.then(() => undefined),
  async ask(prompt: string, mode?: string): Promise<string> {
    const bridge = await bridgePromise;
    return bridge.ask(prompt, mode);
  },
  async reset(): Promise<void> {
    const bridge = await bridgePromise;
    await bridge.reset();
  },
};

export default kolibriBridge;
