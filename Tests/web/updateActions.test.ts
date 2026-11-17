import assert from 'node:assert/strict';
import { test } from 'node:test';
import type { UpdateBackend, UpdateStatus } from '../../web/backend.ts';
import type { MenuItem } from '../../web/editor/Menu.tsx';
import { updateMenuItems } from '../../web/editor/updateActions.ts';

const backendWith = (status: UpdateStatus) => {
  const calls: string[] = [];
  const updates: UpdateBackend = {
    status: () => status,
    onStatus: () => () => {},
    openPage: () => void calls.push('openPage'),
    setChecking: (checking) => void calls.push(`setChecking ${checking}`),
  };
  return { updates, calls };
};

const labels = (items: MenuItem[]) => items.map((item) => (item.kind === 'separator' ? '-' : item.label));

const select = (items: MenuItem[], label: string) => {
  const item = items.find((entry) => entry.kind === 'item' && entry.label === label);
  assert.ok(item?.kind === 'item');
  item.onSelect?.();
};

// As Tests/UpdateTests.cpp
test('the menu has the notice only while there is an update, and always the switch', () => {
  const upToDate = updateMenuItems(backendWith({ notice: '', checking: true }).updates);
  assert.deepEqual(labels(upToDate), ['-', 'Check for updates']);

  const withUpdate = updateMenuItems(backendWith({ notice: 'v1.3.0 available', checking: true }).updates);
  assert.deepEqual(labels(withUpdate), ['-', 'v1.3.0 available', 'Check for updates']);
});

test('the switch is ticked while checking, and choosing it turns checking the other way', () => {
  for (const checking of [true, false]) {
    const { updates, calls } = backendWith({ notice: '', checking });
    const items = updateMenuItems(updates);
    const switchItem = items.at(-1);

    assert.ok(switchItem?.kind === 'item');
    assert.equal(switchItem.ticked, checking);

    select(items, 'Check for updates');
    assert.deepEqual(calls, [`setChecking ${!checking}`]);
  }
});

test('choosing the notice opens the plugin\'s page', () => {
  const { updates, calls } = backendWith({ notice: 'v1.3.0 available', checking: true });

  select(updateMenuItems(updates), 'v1.3.0 available');
  assert.deepEqual(calls, ['openPage']);
});

test('a plugin that doesn\'t check adds nothing to the menu', () => {
  assert.deepEqual(updateMenuItems(undefined), []);
});
