import assert from 'node:assert/strict';
import { test } from 'node:test';
import { localPresets, type PresetStorage } from '../../web/demo/localPresets.ts';
import { memoryStorage } from '../../web/testing/presets.ts';
import { presetActions } from '../../web/editor/presetActions.ts';
import type { Prompt } from '../../web/editor/PromptOverlay.tsx';

const presetsIn = (storage: PresetStorage) =>
  localPresets<object>({ storageKey: 'test-presets', storage, factory: [], parse: () => ({}), init: {}, capture: () => ({}), apply: () => {} });

// As Tests/PresetTests.cpp
test('choosing a preset that won\'t load says so, and keeps the current one', async () => {
  const storage = memoryStorage();
  const presets = presetsIn(storage);
  const messages: string[] = [];
  const prompt: Prompt = { askForText: () => {}, askToConfirm: () => {}, showMessage: (_title, message) => void messages.push(message) };
  const actions = presetActions(presets, prompt);

  await presets.save('A', '');
  await presets.save('B', '');
  const [presetA, presetB] = await presets.list();
  assert.ok(presetA && presetB);

  await actions.load(presetA);
  assert.deepEqual(messages, []);

  const otherTab = presetsIn(storage);
  assert.equal(await otherTab.remove(presetB), null);

  await actions.load(presetB);
  assert.deepEqual(messages, ['"B" couldn\'t be loaded.']);
  assert.equal(presets.status().current.name, 'A');
});
