// The helpers a plugin's engine tests use
import assert from 'node:assert/strict';
import { test } from 'node:test';
import { maxError, sampleRate, sine } from '../../web/testing/engine.ts';

test('a sine has the length, level and frequency asked for', () => {
  const wave = sine(0.5, 0.25, 1000);

  assert.equal(wave.length, sampleRate / 2);
  assert.equal(wave[0], 0);
  // A quarter of the way through a 48-sample cycle
  assert.ok(Math.abs(wave[12]! - 0.25) < 1e-6);
  assert.ok(Math.abs(wave[36]! + 0.25) < 1e-6);
});

test('the largest error is found in a signal of any length', () => {
  const expected = sine(5, 0.5);
  const actual = Float32Array.from(expected);
  actual[200000] = expected[200000]! + 0.125;

  assert.equal(maxError(expected, expected), 0);
  assert.ok(Math.abs(maxError(actual, expected) - 0.125) < 1e-6);
});
