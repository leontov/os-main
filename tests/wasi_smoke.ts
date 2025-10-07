import { createWasiContext } from "../frontend/src/core/wasi";

async function run(): Promise<void> {
  const bytes = Uint8Array.from([
    0x00,
    0x61,
    0x73,
    0x6d,
    0x01,
    0x00,
    0x00,
    0x00,
    0x05,
    0x03,
    0x01,
    0x00,
    0x01,
    0x07,
    0x0a,
    0x01,
    0x06,
    0x6d,
    0x65,
    0x6d,
    0x6f,
    0x72,
    0x79,
    0x02,
    0x00,
  ]);

  const stdout: string[] = [];
  const wasi = createWasiContext((text) => stdout.push(text));
  const wasiImports = wasi.imports as Record<string, (fd: number, statPtr: number) => number>;
  const fdFdstatGet = wasiImports.fd_fdstat_get as (fd: number, statPtr: number) => number;

  const errnoBefore = fdFdstatGet(0, 0);
  if (errnoBefore !== 28) {
    throw new Error(`fd_fdstat_get before memory init should return 28, got ${errnoBefore}`);
  }

  const importObject: WebAssembly.Imports = {
    wasi_snapshot_preview1: wasi.imports,
  };

  const { instance } = await WebAssembly.instantiate(bytes, importObject);
  const exports = instance.exports as { memory: WebAssembly.Memory };
  wasi.setMemory(exports.memory);

  const targetPtr = 128;
  const errnoAfter = fdFdstatGet(0, targetPtr);
  if (errnoAfter !== 0) {
    throw new Error(`fd_fdstat_get after memory init should succeed, got ${errnoAfter}`);
  }

  const view = new DataView(exports.memory.buffer);
  const fileType = view.getUint8(targetPtr);
  if (fileType !== 2) {
    throw new Error(`fd_fdstat_get should write filetype=2, got ${fileType}`);
  }

  for (let offset = 1; offset < 24; offset += 1) {
    if (view.getUint8(targetPtr + offset) !== 0) {
      throw new Error(`fd_fdstat_get should zero fdstat, byte ${offset} was non-zero`);
    }
  }

  if (stdout.length !== 0) {
    throw new Error(`wasi stdout should remain empty during smoke test`);
  }

  console.log("WASI smoke test passed: fd_fdstat_get handles missing memory and instantiation succeeded.");
}

run().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
