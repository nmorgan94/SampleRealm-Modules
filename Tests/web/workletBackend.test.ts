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
