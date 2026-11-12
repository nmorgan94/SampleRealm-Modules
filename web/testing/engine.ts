// A built engine under Node, driven as the demo's worklet drives it
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { instantiateEngine, readUiInfo } from '../demo/engine.ts';

export const sampleRate = 48000;
export const blockSize = 128;

// Returns a function that makes a fresh engine, prepared for that many channels
export function engineAt(wasmPath: string) {
  const wasm = new WebAssembly.Module(readFileSync(wasmPath));

  return (numChannels = 2) => {
    const engine = instantiateEngine(wasm);
    engine.prepare(sampleRate, numChannels);

    const info = readUiInfo(engine);
    const ids = info.params.map((param) => param.id);

    const set = (id: string, value: number) => {
      const index = ids.indexOf(id);
      assert.ok(index >= 0, `no parameter ${id}`);
      engine.setParam(index, value);
    };

    const view = (channel: number, length: number) => new Float32Array(engine.memory.buffer, engine.channel(channel), length);

    // One block at a time, written in and read back from the engine's channels
    const processBlocks = (inputs: Float32Array[]) => {
      const outputs = inputs.map((input) => new Float32Array(input.length));
      const numSamples = inputs[0]?.length ?? 0;

      for (let start = 0; start < numSamples; start += blockSize) {
        const length = Math.min(blockSize, numSamples - start);
        inputs.forEach((input, channel) => view(channel, length).set(input.subarray(start, start + length)));
        engine.render(length);
        outputs.forEach((output, channel) => output.set(view(channel, length), start));
      }

      return outputs;
    };

    // A synth: nothing written in, each channel read back
    const renderBlocks = (numSamples: number) => processBlocks(Array.from({ length: numChannels }, () => new Float32Array(numSamples)));

    // `engine` is the wasm's own exports, for what this doesn't wrap: `engine.midi(status, data1, data2)`
    return { engine, info, set, view, processBlocks, renderBlocks };
  };
}

// `rate` for an engine prepared at another sample rate
export const sine = (seconds: number, amplitude: number, frequency = 440, rate = sampleRate) =>
  Float32Array.from({ length: Math.round(seconds * rate) }, (_, sample) => amplitude * Math.sin((2 * Math.PI * frequency * sample) / rate));

export const maxError = (actual: ArrayLike<number>, expected: ArrayLike<number>) =>
  Array.from(actual).reduce((worst, sample, index) => Math.max(worst, Math.abs(sample - expected[index]!)), 0);
