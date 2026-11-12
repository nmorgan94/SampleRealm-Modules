import assert from 'node:assert/strict';
import { test } from 'node:test';
import type { ParamInfo } from '../../web/backend.ts';
import { formatValue, fromProportion, parseValue, snap, toProportion } from '../../web/editor/params.ts';

const gain: ParamInfo = {
  id: 'gain', name: 'Gain', kind: 'real', min: -60, max: 6, defaultValue: 0, step: 0.1, skew: 1, unit: 'dB', choices: [],
};

const mute: ParamInfo = {
  id: 'mute', name: 'Mute', kind: 'toggle', min: 0, max: 1, defaultValue: 0, step: 1, skew: 1, unit: '', choices: [],
};

test('a toggle snaps to off or on, and reads as juce::AudioParameterBool does', () => {
  assert.equal(snap(mute, 0.7), 1);
  assert.equal(snap(mute, 0.2), 0);
  assert.equal(formatValue(mute, 0), 'Off');
  assert.equal(formatValue(mute, 1), 'On');
});

test('knob positions map to values and back', () => {
  assert.equal(fromProportion(gain, 0.13), -51.4);
  assert.ok(Math.abs(toProportion(gain, fromProportion(gain, 0.25)) - 0.25) < 0.001);
});

test('positions past either end clamp to the range', () => {
  assert.equal(fromProportion(gain, 1.5), 6);
  assert.equal(fromProportion(gain, -0.5), -60);
});

test('typed text parses with its unit, and text without a number is ignored', () => {
  assert.equal(parseValue(gain, '-12 dB'), -12);
  assert.equal(parseValue(gain, '3.14'), 3.1);
  assert.equal(parseValue(gain, '99'), 6);
  assert.equal(parseValue(gain, 'loud'), null);
});
