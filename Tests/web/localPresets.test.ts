// localPresets with a made-up plugin
import assert from 'node:assert/strict';
import { test } from 'node:test';
import { localPresets, parsePreset, xmlTags } from '../../web/demo/localPresets.ts';
import { memoryStorage, presetXml } from '../../web/testing/presets.ts';

type State = { values: Record<string, number> };

const KEY = 'test-presets';
const DEFAULTS = { gain: 0, tone: 5 };

const parse = (xml: string): State => ({
  values: Object.fromEntries(xmlTags(xml, 'PARAM').map(({ id, value }) => [id, Number(value)])),
});

const preset = (name: string, category: string, gain: number) => presetXml(name, category, { gain });

const factory = [preset('Loud', 'Level', 6), preset('Quiet', 'Level', -12), preset('Dark', 'Colour', -3)];

const FACTORY_NAMES = ['Dark', 'Loud', 'Quiet'];

const setUp = (storage = memoryStorage()) => {
  let live: Record<string, number> = { ...DEFAULTS };
  const presets = localPresets<State>({
    storageKey: KEY,
    storage,
    factory,
    parse,
    init: { values: {} },
    capture: () => ({ values: { ...live } }),
    apply: ({ values }) => void (live = { ...DEFAULTS, ...values }),
  });
  const set = (id: string, value: number) => {
    live[id] = value;
    presets.markModified();
  };
  return { presets, storage, set, gain: () => live.gain };
};

const named = <T extends { name: string }>(list: T[], name: string) => {
  const found = list.find((entry) => entry.name === name);
  assert.ok(found, `No preset ${name}`);
  return found;
};

test('a preset file is read with its name, category and the plugin\'s state', () => {
  assert.deepEqual(parsePreset(preset('Quiet', 'Level', -12), parse), { name: 'Quiet', category: 'Level', isFactory: true, values: { gain: -12 } });
  assert.throws(() => parsePreset('<STATE/>', parse), /Not a preset/);
});

test('names keep the characters XmlElement writes by number', () => {
  assert.equal(parsePreset('<PRESET name="Caf&#233; &amp; Bar" category="Keys"/>', parse).name, 'Café & Bar');
});

test('factory presets are listed by category, then name', async () => {
  const { presets } = setUp();
  assert.deepEqual((await presets.list()).map((entry) => entry.name), FACTORY_NAMES);
});

test('a loaded preset applies its state and shows its name', async () => {
  const { presets, gain } = setUp();
  assert.equal(presets.status().current.name, 'Init');

  await presets.load(named(await presets.list(), 'Quiet'));

  assert.equal(gain(), -12);
  assert.deepEqual(presets.status(), { current: { name: 'Quiet', category: 'Level', isFactory: true }, modified: false });
});

test('an edit marks the preset modified until the next load', async () => {
  const { presets, set, gain } = setUp();
  const loud = named(await presets.list(), 'Loud');
  await presets.load(loud);

  const seen: boolean[] = [];
  presets.onStatus((status) => seen.push(status.modified));

  set('gain', -1);
  assert.equal(presets.status().modified, true);

  await presets.load(loud);
  assert.equal(presets.status().modified, false);
  assert.equal(gain(), 6);
  assert.deepEqual(seen, [true, false]);
});

test('Init applies the init state', async () => {
  const { presets, set, gain } = setUp();
  await presets.load(named(await presets.list(), 'Loud'));
  set('gain', 4);

  await presets.loadInit();

  assert.equal(gain(), DEFAULTS.gain);
  assert.deepEqual(presets.status(), { current: { name: 'Init', category: '', isFactory: true }, modified: false });
});

test('next and previous wrap, and start from either end after Init', async () => {
  const { presets, gain } = setUp();
  const name = () => presets.status().current.name;

  await presets.loadNext();
  assert.equal(name(), FACTORY_NAMES[0]);

  await presets.loadPrevious();
  assert.equal(name(), FACTORY_NAMES.at(-1));

  await presets.loadNext();
  assert.equal(name(), FACTORY_NAMES[0]);

  await presets.loadInit();
  await presets.loadPrevious();
  assert.equal(name(), FACTORY_NAMES.at(-1));
  assert.equal(gain(), -12);
});

test('user presets save, rename and delete, and outlast the page', async () => {
  const { presets, set, storage } = setUp();
  set('gain', -3);

  assert.equal(await presets.save('Mine', 'Level'), null);
  assert.deepEqual(presets.status(), { current: { name: 'Mine', category: 'Level', isFactory: false }, modified: false });
  assert.equal(await presets.exists('mine'), true);
  assert.deepEqual((await presets.list()).map((entry) => entry.name), [...FACTORY_NAMES, 'Mine']);

  // The state is stored beside the name and category
  assert.deepEqual(JSON.parse(storage.items.get(KEY)!), [{ name: 'Mine', category: 'Level', values: { gain: -3, tone: 5 } }]);

  // A new page reads it back
  const reopened = setUp(storage);
  await reopened.presets.load(named(await reopened.presets.list(), 'Mine'));
  assert.equal(reopened.gain(), -3);

  assert.equal(await presets.rename(named(await presets.list(), 'Mine'), 'Yours'), null);
  assert.equal(presets.status().current.name, 'Yours');
  assert.deepEqual((await presets.list()).filter((entry) => !entry.isFactory).map((entry) => entry.name), ['Yours']);

  assert.equal(await presets.remove(named(await presets.list(), 'Yours')), null);
  assert.equal(await presets.exists('Yours'), false);
  assert.equal(presets.status().modified, true);
  assert.equal(storage.items.get(KEY), '[]');
});

test('the current preset is reported while it\'s listed', async () => {
  const { presets } = setUp();
  assert.equal(await presets.current(), null);

  await presets.save('Mine', 'Level');
  assert.deepEqual(await presets.current(), named(await presets.list(), 'Mine'));

  await presets.remove(named(await presets.list(), 'Mine'));
  assert.equal(await presets.current(), null);
});

test('user presets can\'t take a factory name', async () => {
  const { presets } = setUp();

  assert.match((await presets.save('loud', '')) ?? '', /factory preset/);
  assert.equal(await presets.save('   ', ''), 'Enter a preset name.');
  assert.equal(await presets.exists('loud'), false);

  assert.equal(await presets.save('Mine', ''), null);
  assert.match((await presets.rename(named(await presets.list(), 'Mine'), 'Quiet')) ?? '', /factory preset/);
  assert.equal(await presets.exists('Mine'), true);
});

test('next and previous skip a user preset deleted in another tab', async () => {
  const { presets, storage } = setUp();
  await presets.save('A', '');
  await presets.save('B', '');
  await presets.load(named(await presets.list(), 'A'));

  const otherTab = setUp(storage).presets;
  assert.equal(await otherTab.remove(named(await otherTab.list(), 'B')), null);

  await presets.loadNext();
  assert.equal(presets.status().current.name, FACTORY_NAMES[0]);
});

test('saving, renaming and deleting keep what another tab saved', async () => {
  const { presets, storage } = setUp();
  await presets.save('Mine', '');

  const otherTab = setUp(storage).presets;
  assert.equal(await otherTab.save('Theirs', ''), null);

  const users = () => JSON.parse(storage.items.get(KEY)!).map((stored: { name: string }) => stored.name);

  assert.equal(await presets.save('Mine', ''), null);
  assert.deepEqual(users(), ['Mine', 'Theirs']);

  assert.equal(await presets.rename({ name: 'Mine', category: '', isFactory: false }, 'Ours'), null);
  assert.deepEqual(users(), ['Ours', 'Theirs']);

  assert.equal(await presets.remove({ name: 'Ours', category: '', isFactory: false }), null);
  assert.deepEqual(users(), ['Theirs']);
});

test('without storage, user presets last until the page closes', async () => {
  const { presets } = setUp({ ...memoryStorage(), setItem: () => { throw new Error('Refused'); } });

  assert.equal(await presets.save('Mine', ''), null);
  assert.equal(await presets.exists('Mine'), true);
  assert.deepEqual(await presets.current(), named(await presets.list(), 'Mine'));
});

test('stored entries that aren\'t named presets are left out, and the rest still work', async () => {
  const storage = memoryStorage();
  storage.setItem(KEY, JSON.stringify([{}, null, 'text', { name: 7 }, { name: 'Kept', values: { gain: -3 } }]));
  const { presets, gain } = setUp(storage);

  assert.deepEqual((await presets.list()).filter((entry) => !entry.isFactory), [{ name: 'Kept', category: '', isFactory: false, values: { gain: -3 } }]);
  assert.equal(await presets.exists('Kept'), true);

  await presets.load(named(await presets.list(), 'Kept'));
  assert.equal(gain(), -3);

  assert.equal(await presets.save('Mine', ''), null);
  assert.equal(await presets.rename({ name: 'Kept', category: '', isFactory: false }, 'Renamed'), null);
  assert.equal(await presets.remove({ name: 'Renamed', category: '', isFactory: false }), null);
  assert.deepEqual(JSON.parse(storage.items.get(KEY)!).map((stored: { name: string }) => stored.name), ['Mine']);
});

test('unreadable storage leaves only the factory presets', async () => {
  const storage = memoryStorage();
  storage.setItem(KEY, 'not json');
  const { presets } = setUp(storage);
  assert.deepEqual((await presets.list()).map((entry) => entry.name), FACTORY_NAMES);
});
