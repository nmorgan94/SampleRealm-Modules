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
