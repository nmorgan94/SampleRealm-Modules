// The same cases as the layout tests in Tests/LabeledSliderTests.cpp
import assert from 'node:assert/strict';
import { test } from 'node:test';
import { layOutAboveLabel, layOutBelowLabel } from '../../web/editor/layout.ts';

test('a label above has the control taking the room under it', () => {
  assert.deepEqual(layOutBelowLabel(74, 115), {
    control: { left: 0, top: 24, width: 74, height: 91 },
    label: { left: 0, top: 0, width: 74, height: 24 },
  });
});

test('the label goes right under a square control, the pair centred', () => {
  assert.deepEqual(layOutAboveLabel(140, 190), {
    control: { left: 0, top: 13, width: 140, height: 140 },
    label: { left: 0, top: 153, width: 140, height: 24 },
  });
  assert.deepEqual(layOutAboveLabel(100, 101), {
    control: { left: 11, top: 0, width: 77, height: 77 },
    label: { left: 0, top: 77, width: 100, height: 24 },
  });
  assert.deepEqual(layOutAboveLabel(50, 21), {
    control: { left: 25, top: -1, width: 0, height: 0 },
    label: { left: 0, top: -1, width: 50, height: 24 },
  });
});
