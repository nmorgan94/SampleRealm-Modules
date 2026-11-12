import assert from 'node:assert/strict';
import { test } from 'node:test';
import type { ParamInfo } from '../../web/backend.ts';
import { clamp01, formatValue, fromProportion, parseValue, rangeFromProportion, snap, toProportion } from '../../web/editor/params.ts';

const gain: ParamInfo = {
  id: 'gain', name: 'Gain', kind: 'real', min: -60, max: 6, defaultValue: 0, step: 0.1, skew: 1, unit: 'dB', choices: [],
};

const mute: ParamInfo = {
  id: 'mute', name: 'Mute', kind: 'toggle', min: 0, max: 1, defaultValue: 0, step: 1, skew: 1, unit: '', choices: [],
};

const voices: ParamInfo = {
  id: 'voices', name: 'Voices', kind: 'integer', min: 1, max: 8, defaultValue: 1, step: 1, skew: 1, unit: '', choices: [],
};

const shape: ParamInfo = {
  id: 'shape', name: 'Shape', kind: 'choice', min: 0, max: 2, defaultValue: 0, step: 1, skew: 1, unit: '', choices: ['Sine', 'Saw', 'Square'],
};

const rate: ParamInfo = {
  id: 'rate', name: 'Rate', kind: 'choice', min: 0, max: 3, defaultValue: 0, step: 1, skew: 1, unit: '', choices: ['1/16', '1/8', '1/4', '1 Bar'],
};

test('a choice reads back its own names, in any case, before numbers', () => {
  rate.choices.forEach((choice, index) => {
    assert.equal(parseValue(rate, formatValue(rate, index)), index, choice);
    assert.equal(parseValue(rate, choice.toUpperCase()), index, choice.toUpperCase());
  });
  assert.equal(parseValue(rate, ' 1/4 '), 2);
  assert.equal(parseValue(shape, '2'), 2);
  assert.equal(parseValue(shape, 'Triangle'), null);
});

test('integer and choice values round halves to even, as the plugin parameters do', () => {
  assert.equal(fromProportion(voices, 0.5), 4);
  assert.equal(snap(voices, 5.5), 6);
  assert.equal(snap(voices, 5.4), 5);
  assert.equal(snap(voices, 12), 8);
  assert.equal(snap(shape, 0.5), 0);
  assert.equal(snap(shape, 1.5), 2);
  assert.equal(formatValue(shape, snap(shape, 1.5)), 'Square');
});

test('a toggle snaps to off or on, and reads as juce::AudioParameterBool does', () => {
  assert.equal(snap(mute, 0.7), 1);
  assert.equal(snap(mute, 0.5), 1);
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
  assert.equal(clamp01(-0.2), 0);
  assert.equal(clamp01(1.2), 1);
  assert.equal(clamp01(NaN), 0);
});

test('a position maps to the range unsnapped, or snapped to the step as a knob moves', () => {
  const skewed = { ...gain, skew: 0.5 };
  assert.ok(Math.abs(rangeFromProportion(gain, 0.13) - -51.42) < 1e-9);
  assert.ok(Math.abs(rangeFromProportion(skewed, 0.5) - -43.5) < 1e-9);
  assert.equal(fromProportion(skewed, 0.5), -43.5);
  assert.equal(rangeFromProportion(skewed, 0), -60);
});

test('a step gives the decimal places of its text', () => {
  assert.deepEqual([0.001, 0.1, 0.01, 1].map((step) => formatValue({ ...gain, step }, 1)), ['1.000', '1.0', '1.00', '1']);
});

test('typed text parses with its unit, and text without a number is ignored', () => {
  assert.equal(parseValue(gain, '-12 dB'), -12);
  assert.equal(parseValue(gain, '3.14'), 3.1);
  assert.equal(parseValue(gain, '99'), 6);
  assert.equal(parseValue(gain, 'loud'), null);
});
