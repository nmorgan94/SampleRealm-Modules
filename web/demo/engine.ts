import type { UiInfo } from '../backend.ts';

export type Engine = Record<string, CallableFunction> & { memory: WebAssembly.Memory };

// As processor.js does in the worklet
export function instantiateEngine(module: WebAssembly.Module) {
  // WASI imports are never called
  const stub = new Proxy({}, { get: () => () => 0 });
  const engine = new WebAssembly.Instance(module, new Proxy({}, { get: () => stub })).exports as Engine;
  engine._initialize?.();
  return engine;
}

export const readUiInfo = (engine: Engine) => {
  const bytes = new Uint8Array(engine.memory.buffer, engine.uiInfo());
  return JSON.parse(new TextDecoder().decode(bytes.subarray(0, bytes.indexOf(0)))) as UiInfo;
};

export async function loadEngine(url: string): Promise<{ wasm: ArrayBuffer; info: UiInfo }> {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`${url}: ${response.status}`);

  const wasm = await response.arrayBuffer();
  return { wasm, info: readUiInfo(instantiateEngine(await WebAssembly.compile(wasm))) };
}

// processorUrl is processor.js. Resolves once the processor has built its engine
export async function startWorklet(context: AudioContext, processorUrl: string, options: AudioWorkletNodeOptions) {
  await context.audioWorklet.addModule(processorUrl);
  // As processor.js registers it
  const node = new AudioWorkletNode(context, 'engine-processor', options);

  await new Promise<void>((resolve, reject) => {
    node.port.onmessage = ({ data }) => {
      if (data.type === 'ready') resolve();
      else if (data.type === 'error') reject(new Error(data.message));
    };
    // A processor that throws posts nothing
    node.onprocessorerror = () => reject(new Error('The audio processor failed'));
  });

  return node;
}
