import assert from 'node:assert/strict';
import { test } from 'node:test';
import type { MenuItem } from '../../web/editor/Menu.tsx';
import type { Prompt } from '../../web/editor/PromptOverlay.tsx';
import { screenshotMenuItems } from '../../web/editor/screenshotActions.ts';

const promptRecording = (messages: string[]): Prompt => ({
  askForText: () => {},
  askToConfirm: () => {},
  showMessage: (title, message) => void messages.push(`${title}: ${message}`),
});

const choose = (items: MenuItem[]) => {
  const [item] = items;
  assert.ok(item?.kind === 'item' && item.label === 'Screenshot');
  item.onSelect?.();
};

// Lets the chosen item's promises finish
const settled = () => new Promise((resolve) => setImmediate(resolve));

test('the menu has Screenshot only where a plugin can save one', () => {
  assert.deepEqual(screenshotMenuItems({}, promptRecording([])), []);
  assert.equal(screenshotMenuItems({ screenshot: async () => null }, promptRecording([])).length, 1);
});

test('the picture is taken only once the menu has gone', async () => {
  const order: string[] = [];
  const backend = { screenshot: async () => (order.push('screenshot'), null) };
  const menuGone = async () => void order.push('menu gone');

  choose(screenshotMenuItems(backend, promptRecording([]), menuGone));
  await settled();

  assert.deepEqual(order, ['menu gone', 'screenshot']);
});

test('a screenshot that couldn\'t be saved says why, and one that was says nothing', async () => {
  for (const [error, expected] of [['The disk is full.', ['Screenshot: The disk is full.']], [null, []]] as const) {
    const messages: string[] = [];

    choose(screenshotMenuItems({ screenshot: async () => error }, promptRecording(messages), async () => {}));
    await settled();

    assert.deepEqual(messages, expected);
  }
});

test('a screenshot call that fails says so', async () => {
  const messages: string[] = [];
  const backend = { screenshot: () => Promise.reject<string | null>(new Error('no bridge')) };

  choose(screenshotMenuItems(backend, promptRecording(messages), async () => {}));
  await settled();

  assert.deepEqual(messages, ["Screenshot: The screenshot couldn't be taken."]);
});
