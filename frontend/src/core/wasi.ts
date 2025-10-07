const WASI_ERRNO_SUCCESS = 0;
const WASI_ERRNO_BADF = 8;
const WASI_ERRNO_INVAL = 28;
const WASI_FILETYPE_CHARACTER_DEVICE = 2;

const FDSTAT_SIZE = 24;


interface WasiContext {
  imports: Record<string, WebAssembly.ImportValue>;
  setMemory(memory: WebAssembly.Memory): void;
}

const textDecoder = new TextDecoder("utf-8");

function getRandomBytes(buffer: Uint8Array): void {
  if (typeof crypto !== "undefined" && "getRandomValues" in crypto) {
    crypto.getRandomValues(buffer);
    return;
  }
  for (let index = 0; index < buffer.length; index += 1) {
    buffer[index] = Math.floor(Math.random() * 256);
  }
}

export function createWasiContext(onStdout?: (text: string) => void): WasiContext {
  let memory: WebAssembly.Memory | null = null;

  let memoryView: DataView | null = null;

  const ensureView = (): DataView => {
    if (!memoryView) {
      throw new Error("WASI memory не инициализирована");
    }
    return memoryView;

  };

  const readBytes = (ptr: number, length: number): Uint8Array => {
    if (!memory) {
      throw new Error("WASI memory не инициализирована");
    }
    return new Uint8Array(memory.buffer, ptr, length);
  };

  const writeU32 = (ptr: number, value: number): void => {

    if (!memoryView) {
      return;
    }
    memoryView.setUint32(ptr, value >>> 0, true);
  };

  const writeU64 = (ptr: number, value: bigint): void => {
    if (!memoryView) {
      return;
    }
    memoryView.setBigUint64(ptr, value, true);

  };

  const imports: Record<string, WebAssembly.ImportValue> = {
    fd_close: (fd: number): number => {
      void fd;
      return WASI_ERRNO_SUCCESS;
    },
    fd_fdstat_get: (fd: number, statPtr: number): number => {
      if (fd < 0) {
        return WASI_ERRNO_BADF;
      }

      if (!memory || !memoryView) {
        return WASI_ERRNO_INVAL;
      }
      const view = memoryView;
      const buffer = new Uint8Array(memory.buffer, statPtr, FDSTAT_SIZE);
      buffer.fill(0);

      view.setUint8(statPtr, WASI_FILETYPE_CHARACTER_DEVICE);
      view.setUint16(statPtr + 2, 0, true);
      view.setBigUint64(statPtr + 8, 0n, true);
      view.setBigUint64(statPtr + 16, 0n, true);
      return WASI_ERRNO_SUCCESS;
    },
    fd_seek: (
      fd: number,
      offset: number | bigint,
      whence: number,
      resultPtr: number,
    ): number => {
      void fd;
      void offset;
      void whence;
      writeU64(resultPtr, 0n);
      return WASI_ERRNO_SUCCESS;
    },
    fd_write: (fd: number, iovsPtr: number, iovsLen: number, nwrittenPtr: number): number => {

      if (!memory || !memoryView) {
        return WASI_ERRNO_INVAL;
      }
      const view = ensureView();

      let bytesWritten = 0;
      let aggregated = "";
      for (let index = 0; index < iovsLen; index += 1) {
        const ptr = view.getUint32(iovsPtr + index * 8, true);
        const len = view.getUint32(iovsPtr + index * 8 + 4, true);
        if (len === 0) {
          continue;
        }
        bytesWritten += len;
        if (fd === 1 || fd === 2) {
          const chunk = readBytes(ptr, len);
          aggregated += textDecoder.decode(chunk);
        }
      }
      writeU32(nwrittenPtr, bytesWritten);
      if (aggregated && onStdout) {
        onStdout(aggregated);
      }
      return WASI_ERRNO_SUCCESS;
    },
    environ_sizes_get: (countPtr: number, sizePtr: number): number => {
      writeU32(countPtr, 0);
      writeU32(sizePtr, 0);
      return WASI_ERRNO_SUCCESS;
    },
    environ_get: (): number => WASI_ERRNO_SUCCESS,
    args_sizes_get: (countPtr: number, sizePtr: number): number => {
      writeU32(countPtr, 0);
      writeU32(sizePtr, 0);
      return WASI_ERRNO_SUCCESS;
    },
    args_get: (): number => WASI_ERRNO_SUCCESS,
    clock_time_get: (
      clockId: number,
      precision: number | bigint,
      timePtr: number,
    ): number => {
      void clockId;
      void precision;
      const now = BigInt(Date.now()) * 1_000_000n;
      writeU64(timePtr, now);
      return WASI_ERRNO_SUCCESS;
    },
    random_get: (ptr: number, len: number): number => {
      if (!memory) {
        return WASI_ERRNO_INVAL;
      }

      const buffer = new Uint8Array(memory.buffer, ptr, len);

      getRandomBytes(buffer);
      return WASI_ERRNO_SUCCESS;
    },
    proc_exit: (code: number): number => {
      throw new Error(`WASM завершил выполнение с кодом ${code}`);
    },
  };

  return {
    imports,
    setMemory(newMemory: WebAssembly.Memory): void {
      memory = newMemory;
      memoryView = new DataView(newMemory.buffer);

    },
  };
}
