// workletBackend with a made-up plugin
import assert from 'node:assert/strict';
import { test } from 'node:test';
import type { ParamInfo, UiInfo } from '../../web/backend.ts';
import { workletBackend, type EnginePort } from '../../web/demo/workletBackend.ts';
import { memoryStorage, presetXml } from '../../web/testing/presets.ts';

const KEY = 'test-presets';

const param = (id: string, defaultValue: number): ParamInfo =>
  ({ id, name: id, kind: 'real', min: -60, max: 12, defaultValue, step: 0.1, skew: 1, unit: 'dB', choices: [] });

const info: UiInfo = { version: '0.0.1', commit: 'abc1234', build: 'Debug', params: [param('gain', 0), param('tone', 5)] };

const preset = (name: string, gain: number) => presetXml(name, 'Level', { gain });

const fakePort = () => {
  const posted: unknown[] = [];
  const port: EnginePort = { postMessage: (message: unknown) => void posted.push(message), onmessage: null };
  return { port, posted };
};

const storageWith = (stored: unknown) => memoryStorage({ [KEY]: JSON.stringify(stored) });

test('changes made before the worklet attaches reach it once it does, in order and once each', () => {
  const backend = workletBackend(info, { storageKey: KEY });
  backend.setValue('tone', -12);
  backend.setValue('tone', -3);

  const { port, posted } = fakePort();
  backend.attach(port);
  backend.setValue('tone', 2);

  assert.deepEqual(posted, [
    { type: 'param', index: 1, value: -12 },
    { type: 'param', index: 1, value: -3 },
    { type: 'param', index: 1, value: 2 },
  ]);
});

test('values read back and notify before the worklet attaches', () => {
  const backend = workletBackend(info, { storageKey: KEY });
  const heard: number[] = [];
  backend.subscribe('gain', (value) => heard.push(value));

  assert.equal(backend.getValue('gain'), 0);
  backend.setValue('gain', -6);

  assert.equal(backend.getValue('gain'), -6);
  assert.deepEqual(heard, [-6]);
});

test('the transport and the level pass between the page and the worklet', () => {
  const backend = workletBackend(info, { storageKey: KEY });
  const { port, posted } = fakePort();
  const heard: number[][] = [];
  backend.onLevel(({ peaks }) => heard.push(peaks));

  backend.attach(port);
  backend.setTransport({ bpm: 120, playing: true, ppq: 4 });
  port.onmessage?.(new MessageEvent('message', { data: { type: 'level', peaks: [0.5, 0.25] } }));

  assert.deepEqual(posted, [{ type: 'transport', bpm: 120, playing: true, ppq: 4 }]);
  assert.deepEqual(heard, [[0.5, 0.25]]);
});

test('notes and midi reach a synth\'s worklet as three bytes, those played early once it attaches', () => {
  const backend = workletBackend(info, { storageKey: KEY });
  backend.noteOn(60);

  const { port, posted } = fakePort();
  backend.attach(port);
  backend.noteOn(64, 80);
  backend.noteOff(60);
  backend.midi(Uint8Array.of(0xe0, 0, 64));
  backend.midi([0xc0, 5]);

  assert.deepEqual(posted, [
    { type: 'midi', bytes: [0x90, 60, 100] },
    { type: 'midi', bytes: [0x90, 64, 80] },
    { type: 'midi', bytes: [0x80, 60, 0] },
    { type: 'midi', bytes: [0xe0, 0, 64] },
    { type: 'midi', bytes: [0xc0, 5, 0] },
  ]);
});

test('system messages and bytes with no status never reach the worklet', () => {
  const backend = workletBackend(info, { storageKey: KEY });
  const { port, posted } = fakePort();
  backend.attach(port);

  backend.midi([0xf8]);
  backend.midi([0xfe]);
  backend.midi([0xf0, 0x7e, 0x01, 0x02, 0xf7]);
  backend.midi([60, 100]);
  backend.midi([]);
  backend.midi([0xef, 0, 64]);

  assert.deepEqual(posted, [{ type: 'midi', bytes: [0xef, 0, 64] }]);
});

test('a note or velocity that isn\'t a whole number from 0 to 127 is rounded and kept in range', () => {
  const backend = workletBackend(info, { storageKey: KEY });
  const { port, posted } = fakePort();
  backend.attach(port);

  backend.noteOn(60.4, 0.8);
  backend.noteOn(130, 200);
  backend.noteOff(-3);
  backend.noteOff(127.6);

  assert.deepEqual(posted, [
    { type: 'midi', bytes: [0x90, 60, 1] },
    { type: 'midi', bytes: [0x90, 127, 127] },
    { type: 'midi', bytes: [0x80, 0, 0] },
    { type: 'midi', bytes: [0x80, 127, 0] },
  ]);
});

test('a factory preset sets its parameters, and the rest go to their defaults', async () => {
  const backend = workletBackend(info, { storageKey: KEY, factoryPresets: [preset('Quiet', -12)] });
  backend.setValue('tone', 9);

  const [quiet] = await backend.presets.list();
  assert.ok(quiet);
  await backend.presets.load(quiet);

  assert.equal(backend.getValue('gain'), -12);
  assert.equal(backend.getValue('tone'), 5);

  await backend.presets.loadInit();
  assert.equal(backend.getValue('gain'), 0);
});

test('a saved preset holds every parameter, and an edit marks it modified', async () => {
  const backend = workletBackend(info, { storageKey: KEY, storage: storageWith([]) });
  backend.setValue('gain', -3);
  assert.equal(backend.presets.status().modified, true);

  assert.equal(await backend.presets.save('Mine', ''), null);
  backend.setValue('gain', 6);

  await backend.presets.load({ name: 'Mine', category: '', isFactory: false });
  assert.equal(backend.getValue('gain'), -3);
  assert.equal(backend.getValue('tone'), 5);
});

test('a preset value that isn\'t a number loads as the default', async () => {
  const broken = '<PRESET name="Broken" category=""><PARAM id="gain"/><PARAM id="tone" value="loud"/></PRESET>';
  const stored = [{ name: 'Null', category: '', values: { gain: null } }, { name: 'Bare', category: '' }];
  const backend = workletBackend(info, { storageKey: KEY, factoryPresets: [broken], storage: storageWith(stored) });
  const { port, posted } = fakePort();
  backend.attach(port);

  for (const preset of await backend.presets.list()) {
    backend.setValue('gain', -9);
    backend.setValue('tone', 2);
    posted.length = 0;

    assert.equal(await backend.presets.load(preset), true, preset.name);
    assert.equal(backend.getValue('gain'), 0, preset.name);
    assert.equal(backend.getValue('tone'), 5, preset.name);
    assert.deepEqual(posted, [{ type: 'param', index: 0, value: 0 }, { type: 'param', index: 1, value: 5 }], preset.name);
  }
});

test('a stored preset loads past ids this engine doesn\'t have', async () => {
  const backend = workletBackend(info, { storageKey: KEY, storage: storageWith([{ name: 'Other', category: '', values: { cutoff: 1, gain: -3 } }]) });

  await backend.presets.load({ name: 'Other', category: '', isFactory: false });

  assert.equal(backend.getValue('gain'), -3);
  assert.equal(backend.presets.status().current.name, 'Other');
});

test('a performance control doesn\'t mark the preset modified, isn\'t saved, and resets on load', async () => {
  const bend: ParamInfo = { ...param('bend', 0), isPerformanceControl: true };
  const storage = memoryStorage();
  const backend = workletBackend({ ...info, params: [...info.params, bend] }, { storageKey: KEY, storage });

  backend.setValue('bend', 3);
  assert.equal(backend.presets.status().modified, false);

  assert.equal(await backend.presets.save('Mine', ''), null);
  assert.deepEqual(Object.keys(JSON.parse(storage.items.get(KEY)!)[0].values), ['gain', 'tone']);

  await backend.presets.load({ name: 'Mine', category: '', isFactory: false });
  assert.equal(backend.getValue('bend'), 0);
});

test('a value shown for the engine notifies the editor but isn\'t sent back, and isn\'t an edit', () => {
  const backend = workletBackend(info, { storageKey: KEY });
  const { port, posted } = fakePort();
  backend.attach(port);
  const heard: number[] = [];
  backend.subscribe('gain', (value) => heard.push(value));

  backend.showValue('gain', -6);

  assert.equal(backend.getValue('gain'), -6);
  assert.deepEqual(heard, [-6]);
  assert.deepEqual(posted, []);
  assert.equal(backend.presets.status().modified, false);
});

// A plugin whose presets are typed by a choice, and hold a curve beside the values
const typed = (storage = memoryStorage()) => {
  const voice: ParamInfo = { id: 'voice', name: 'Voice', kind: 'choice', min: 0, max: 1, defaultValue: 0, step: 1, skew: 1, unit: '', choices: ['Kick', 'Snare'] };
  const typedPreset = (name: string, type: string, voiceIndex: number, curve: string) =>
    `<PRESET name="${name}" category="" type="${type}"><PARAM id="voice" value="${voiceIndex}"/><PARAM id="gain" value="-6"/><CURVE points="${curve}"/></PRESET>`;

  let curve = [0, 1];
  const backend = workletBackend<{ curve: number[] }>({ ...info, params: [voice, ...info.params] }, {
    storageKey: KEY,
    storage,
    factoryPresets: [typedPreset('Boom', 'Kick', 0, '0 4'), typedPreset('Crack', 'Snare', 1, '0 8'), typedPreset('Any', '', 0, '0 2')],
    typeParameter: 'voice',
    extraState: {
      parse: (xml) => ({ curve: /points="([^"]*)"/.exec(xml)![1]!.split(' ').map(Number) }),
      capture: () => ({ curve }),
      apply: (state) => void (curve = state.curve ?? [0, 1]),
    },
  });

  const names = async () => (await backend.presets.list()).map((entry) => entry.name);
  return { backend, storage, names, curve: () => curve, setCurve: (next: number[]) => void (curve = next) };
};

test("only the current type's presets and those of none are listed, and stepping stays among them", async () => {
  const { backend, names } = typed();
  assert.deepEqual(await names(), ['Any', 'Boom']);

  backend.setValue('voice', 1);
  assert.deepEqual(await names(), ['Any', 'Crack']);

  await backend.presets.loadPrevious();
  assert.equal(backend.presets.status().current.name, 'Crack');
  assert.equal(await backend.presets.load({ name: 'Boom', category: '', isFactory: true }), false);
});

test("a preset holds the plugin's extra state apart from its own fields, and Init keeps the type", async () => {
  const { backend, storage, names, curve, setCurve } = typed();
  backend.setValue('voice', 1);

  await backend.presets.load({ name: 'Crack', category: '', isFactory: true });
  assert.deepEqual(curve(), [0, 8]);
  assert.equal(backend.getValue('gain'), -6);

  setCurve([0, 3]);
  backend.markModified();
  assert.equal(backend.presets.status().modified, true);

  assert.equal(await backend.presets.save('Mine', ''), null);
  assert.deepEqual(JSON.parse(storage.items.get(KEY)!), [{ values: { voice: 1, gain: -6, tone: 5 }, type: 'Snare', extra: { curve: [0, 3] }, name: 'Mine', category: '' }]);

  await backend.presets.loadInit();
  assert.deepEqual(curve(), [0, 1]);
  assert.equal(backend.getValue('voice'), 1);
  assert.equal(backend.getValue('gain'), 0);
  assert.deepEqual(await names(), ['Any', 'Crack', 'Mine']);

  backend.setValue('voice', 0);
  assert.deepEqual(await names(), ['Any', 'Boom']);
});

test("a plugin's own messages reach the worklet, those sent early once it attaches, and it hears what the worklet posts", () => {
  const backend = workletBackend(info, { storageKey: KEY });
  const heard: unknown[] = [];
  const stop = backend.onMessage((data) => heard.push(data));
  backend.post({ type: 'call', name: 'audition' });

  const { port, posted } = fakePort();
  backend.attach(port);
  backend.post({ type: 'call', name: 'setShape', args: [1, 1], floats: { at: 'shapeNodes', values: [0.5], capacity: 1 } });
  port.onmessage?.(new MessageEvent('message', { data: { type: 'watch', values: { hitCount: 1 } } }));
  stop();
  port.onmessage?.(new MessageEvent('message', { data: { type: 'watch', values: { hitCount: 2 } } }));

  assert.deepEqual(posted, [
    { type: 'call', name: 'audition' },
    { type: 'call', name: 'setShape', args: [1, 1], floats: { at: 'shapeNodes', values: [0.5], capacity: 1 } },
  ]);
  assert.deepEqual(heard, [{ type: 'watch', values: { hitCount: 1 } }]);
});
