// processor.js outside a browser: the worklet's globals and the wasm are stood in for
import assert from 'node:assert/strict';
import { after, before, test } from 'node:test';

const SAMPLE_RATE = 48000;
const FRAMES = 128;

// What a worklet posts to the page, and the handler the page's messages arrive at
class FakeWorkletProcessor {
  posted: Record<string, unknown>[] = [];
  port = {
    postMessage: (message: Record<string, unknown>) => void this.posted.push(message),
    onmessage: null as ((event: { data: unknown }) => void) | null,
  };
}

type Processor = FakeWorkletProcessor & {
  process(inputs: Float32Array[][], outputs: Float32Array[][]): boolean;
};

type Frames = { name: string; size: string; perSecond?: number };

let EngineProcessor: new (options: { processorOptions: { wasm: unknown; channels: number; watch?: string[]; frames?: Frames } }) => Processor;

const realModule = WebAssembly.Module;
const realInstance = WebAssembly.Instance;

before(async () => {
  Object.assign(globalThis, {
    AudioWorkletProcessor: FakeWorkletProcessor,
    sampleRate: SAMPLE_RATE,
    registerProcessor: (name: string, processor: typeof EngineProcessor) => {
      assert.equal(name, 'engine-processor');
      EngineProcessor = processor;
    },
  });

  // The `wasm` a test passes is its fake engine, or something that can't be built
  Object.assign(WebAssembly, {
    Module: function (wasm: unknown) {
      if (wasm === 'broken') throw new Error('Not wasm');
      return { engine: wasm };
    },
    Instance: function (module: { engine: unknown }) {
      return { exports: module.engine };
    },
  });

  // A path TypeScript doesn't follow, as the file has no types
  const processorFile = '../../web/demo/processor.js';
  await import(processorFile);
});

after(() => Object.assign(WebAssembly, { Module: realModule, Instance: realInstance }));

// Doubles what's in its channels, and adds 0.25 while a note is held
function fakeEngine() {
  const memory = new WebAssembly.Memory({ initial: 1 });
  const calls: unknown[][] = [];
  const peaks = [0, 0];
  let notesHeld = 0;
  const channel = (index: number) => 1024 + index * 1024;

  const engine = {
    memory,
    prepare: (...args: number[]) => void calls.push(['prepare', ...args]),
    setParam: (...args: number[]) => void calls.push(['setParam', ...args]),
    setTransport: (...args: unknown[]) => void calls.push(['setTransport', ...args]),
    midi: (status: number, data1: number, data2: number) => {
      calls.push(['midi', status, data1, data2]);
      notesHeld += status === 0x90 ? 1 : -1;
    },
    channel,
    render: (frames: number) => {
      for (const index of [0, 1]) {
        const samples = new Float32Array(memory.buffer, channel(index), frames);
        samples.forEach((sample, position) => (samples[position] = sample * 2 + (notesHeld > 0 ? 0.25 : 0)));
        peaks[index] = Math.max(peaks[index]!, ...samples);
      }
    },
    peak: (index: number) => {
      const peak = peaks[index]!;
      peaks[index] = 0;
      return peak;
    },
  };

  return { engine, calls };
}

const start = (channels = 2, without: ('midi' | 'setTransport' | 'peak')[] = []) => {
  const { engine, calls } = fakeEngine();
  const exports = Object.fromEntries(Object.entries(engine).filter(([name]) => !without.some((missing) => missing === name)));
  const processor = new EngineProcessor({ processorOptions: { wasm: exports, channels } });
  return { processor, calls };
};

const send = (processor: Processor, data: unknown) => processor.port.onmessage?.({ data });

const block = (value: number) => new Float32Array(FRAMES).fill(value);
const stereoOutput = () => [new Float32Array(FRAMES), new Float32Array(FRAMES)];

test('the engine is prepared at the context\'s sample rate, and the page is told it\'s ready', () => {
  const { processor, calls } = start();

  assert.deepEqual(calls, [['prepare', SAMPLE_RATE, 2]]);
  assert.deepEqual(processor.posted, [{ type: 'ready' }]);
});

test('an engine that can\'t be built tells the page why, and renders nothing', () => {
  const processor = new EngineProcessor({ processorOptions: { wasm: 'broken', channels: 2 } });

  assert.deepEqual(processor.posted, [{ type: 'error', message: 'Error: Not wasm' }]);
  assert.equal(processor.process([[]], [stereoOutput()]), false);
});

test('an engine without a meter renders, and no level is sent', () => {
  const { processor } = start(2, ['peak']);
  processor.posted.length = 0;

  for (let rendered = 0; rendered < 16; rendered++) {
    const output = stereoOutput();
    assert.equal(processor.process([[block(0.25), block(0.25)]], [output]), true);
    assert.equal(output[0]![0], 0.5);
  }

  assert.deepEqual(processor.posted, []);
});

test('parameters, the transport and midi reach the engine', () => {
  const { processor, calls } = start();
  calls.length = 0;

  send(processor, { type: 'param', index: 3, value: -6 });
  send(processor, { type: 'transport', bpm: 120, playing: true, ppq: 4 });
  send(processor, { type: 'midi', bytes: [0x90, 60, 100] });

  assert.deepEqual(calls, [['setParam', 3, -6], ['setTransport', 120, true, 4], ['midi', 0x90, 60, 100]]);
});

test('midi sent to an effect, or the transport to a synth, is ignored', () => {
  const effect = start(2, ['midi']);
  const synth = start(2, ['setTransport']);
  effect.calls.length = 0;
  synth.calls.length = 0;

  send(effect.processor, { type: 'midi', bytes: [0x90, 60, 100] });
  send(synth.processor, { type: 'transport', bpm: 120, playing: true, ppq: 4 });
  send(effect.processor, { type: 'param', index: 0, value: 1 });
  send(synth.processor, { type: 'param', index: 0, value: 1 });

  assert.deepEqual(effect.calls, [['setParam', 0, 1]]);
  assert.deepEqual(synth.calls, [['setParam', 0, 1]]);
});

test('an effect\'s input goes through the engine to the output', () => {
  const { processor } = start();
  const output = stereoOutput();

  assert.equal(processor.process([[block(0.25), block(-0.125)]], [output]), true);

  assert.deepEqual([output[0]![0], output[0]![FRAMES - 1], output[1]![0]], [0.5, 0.5, -0.25]);
});

test('a paused input leaves silence, not the last block', () => {
  const { processor } = start();
  processor.process([[block(0.25), block(0.25)]], [stereoOutput()]);

  const output = stereoOutput();
  processor.process([[]], [output]);

  assert.deepEqual([output[0]![0], output[1]![0]], [0, 0]);
});

test('a synth has no input, and plays what its midi asks for', () => {
  const { processor } = start();
  const silent = stereoOutput();
  processor.process([], [silent]);
  assert.deepEqual([silent[0]![0], silent[1]![0]], [0, 0]);

  send(processor, { type: 'midi', bytes: [0x90, 60, 100] });
  const playing = stereoOutput();
  processor.process([], [playing]);
  assert.deepEqual([playing[0]![0], playing[1]![FRAMES - 1]], [0.25, 0.25]);

  send(processor, { type: 'midi', bytes: [0x80, 60, 0] });
  const released = stereoOutput();
  processor.process([], [released]);
  assert.deepEqual([released[0]![0], released[1]![0]], [0, 0]);
});

test('a mono engine\'s channel goes to both outputs', () => {
  const { processor } = start(1);
  const output = stereoOutput();

  processor.process([[block(0.25)]], [output]);

  assert.deepEqual([output[0]![0], output[1]![0]], [0.5, 0.5]);
});

test('the level is posted every eighth block, and silence only once', () => {
  const { processor } = start();
  const levels = () => processor.posted.filter((message) => message.type === 'level');
  const run = (blocks: number, value: number) => {
    for (let count = 0; count < blocks; count++) processor.process([[block(value), block(value / 2)]], [stereoOutput()]);
  };

  run(7, 0.25);
  assert.deepEqual(levels(), []);

  run(1, 0.25);
  assert.deepEqual(levels(), [{ type: 'level', peaks: [0.5, 0.25] }]);

  run(8, 0);
  run(8, 0);
  assert.deepEqual(levels(), [{ type: 'level', peaks: [0.5, 0.25] }, { type: 'level', peaks: [0, 0] }]);
});

// A plugin's own exports: a hit counter, the last note, and a shape set from floats written at a pointer
function pluginEngine() {
  const { engine, calls } = fakeEngine();
  const scratch = 8192;
  let hits = 0;
  let lastNote = 36;

  const exports = {
    ...engine,
    midi: (_status: number, note: number) => {
      hits++;
      lastNote = note;
    },
    hitCount: () => hits,
    lastNote: () => lastNote,
    shapeNodes: () => scratch,
    setShape: (index: number, count: number) => void calls.push(['setShape', index, ...new Float32Array(engine.memory.buffer, scratch, count)]),
    audition: () => void calls.push(['audition']),
  };

  return { exports, calls };
}

test("a call reaches an export of the plugin's own, with its floats written first", () => {
  const { exports, calls } = pluginEngine();
  const processor = new EngineProcessor({ processorOptions: { wasm: exports, channels: 2 } });
  calls.length = 0;

  send(processor, { type: 'call', name: 'audition' });
  send(processor, { type: 'call', name: 'setShape', args: [1, 3], floats: { at: 'shapeNodes', values: [0, 0.5, -1] } });

  assert.deepEqual(calls, [['audition'], ['setShape', 1, 0, 0.5, -1]]);
});

test('a call the engine has no export for is reported and not made', () => {
  const { exports, calls } = pluginEngine();
  const processor = new EngineProcessor({ processorOptions: { wasm: exports, channels: 2 } });
  calls.length = 0;

  send(processor, { type: 'call', name: 'missing' });

  assert.deepEqual(calls, []);
  assert.deepEqual(processor.posted.slice(1), [{ type: 'error', message: 'Error: The engine has no export "missing"' }]);
});

test("a watched name the engine has no export for tells the page why", () => {
  const { exports } = pluginEngine();
  const processor = new EngineProcessor({ processorOptions: { wasm: exports, channels: 2, watch: ['hitCount', 'lastNotes'] } });

  assert.deepEqual(processor.posted, [{ type: 'error', message: 'Error: The engine has no export "lastNotes"' }]);
  assert.equal(processor.process([], [stereoOutput()]), false);
});

test('watched exports are posted together when one changes, and not while they stay the same', () => {
  const { exports } = pluginEngine();
  const processor = new EngineProcessor({ processorOptions: { wasm: exports, channels: 2, watch: ['hitCount', 'lastNote'] } });
  const watched = () => processor.posted.filter((message) => message.type === 'watch');

  processor.process([], [stereoOutput()]);
  assert.deepEqual(watched(), []);

  send(processor, { type: 'midi', bytes: [0x90, 36, 100] });
  processor.process([], [stereoOutput()]);
  processor.process([], [stereoOutput()]);
  assert.deepEqual(watched(), [{ type: 'watch', values: { hitCount: 1, lastNote: 36 } }]);

  send(processor, { type: 'midi', bytes: [0x90, 48, 100] });
  processor.process([], [stereoOutput()]);
  assert.deepEqual(watched().at(-1), { type: 'watch', values: { hitCount: 2, lastNote: 48 } });
});

// A plugin's own frame: the seconds it was asked for, then two floats of its own
function frameEngine() {
  const { engine } = fakeEngine();
  const scratch = 8192;

  return {
    ...engine,
    meterFrame: (seconds: number) => {
      new Float32Array(engine.memory.buffer, scratch, 3).set([seconds, 0.5, -1]);
      return scratch;
    },
    meterFrameSize: () => 3,
  };
}

test("a plugin's frame is posted so many times a second, asked for with the time since the last", () => {
  const frames = { name: 'meterFrame', size: 'meterFrameSize', perSecond: 60 };
  const processor = new EngineProcessor({ processorOptions: { wasm: frameEngine(), channels: 2, frames } });
  const posted = () => processor.posted.filter((message) => message.type === 'frame');
  const run = (blocks: number) => {
    for (let count = 0; count < blocks; count++) processor.process([], [stereoOutput()]);
  };

  // 48 kHz in blocks of 128 is 375 blocks a second: every sixth is nearest to 60 a second
  run(5);
  assert.deepEqual(posted(), []);

  run(6);
  assert.deepEqual(posted(), [{ type: 'frame', frame: Float32Array.of((6 * FRAMES) / SAMPLE_RATE, 0.5, -1) }]);
});

test('a frame the engine has no export for tells the page why', () => {
  const frames = { name: 'meterFrame', size: 'missing', perSecond: 60 };
  const processor = new EngineProcessor({ processorOptions: { wasm: frameEngine(), channels: 2, frames } });

  assert.deepEqual(processor.posted, [{ type: 'error', message: 'Error: The engine has no export "missing"' }]);
});

test('a frame with no size, or no times a second, tells the page why', () => {
  const errorsFrom = (wasm: unknown, perSecond?: number) => {
    const frames = { name: 'meterFrame', size: 'meterFrameSize', perSecond };
    return new EngineProcessor({ processorOptions: { wasm, channels: 2, frames } }).posted;
  };

  assert.deepEqual(errorsFrom({ ...frameEngine(), meterFrameSize: () => 0 }, 60), [
    { type: 'error', message: 'Error: The engine\'s "meterFrameSize" gave a frame of 0 floats' },
  ]);
  assert.deepEqual(errorsFrom(frameEngine()), [{ type: 'error', message: "Error: A frame can't be posted undefined times a second" }]);
});
