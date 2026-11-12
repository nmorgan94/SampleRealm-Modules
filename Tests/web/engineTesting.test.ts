// The helpers a plugin's engine tests use
import assert from 'node:assert/strict';
import { test } from 'node:test';
import { maxError, rms, sampleRate, sine } from '../../web/testing/engine.ts';

test('a sine has the length, level and frequency asked for', () => {
  const wave = sine(0.5, 0.25, 1000);

  assert.equal(wave.length, sampleRate / 2);
  assert.equal(wave[0], 0);
  // A quarter of the way through a 48-sample cycle
  assert.ok(Math.abs(wave[12]! - 0.25) < 1e-6);
  assert.ok(Math.abs(wave[36]! + 0.25) < 1e-6);
});

test("a sine's level is its peak over root two, over the whole signal or part of it", () => {
  const loudThenQuiet = [...sine(1, 0.5), ...sine(1, 0.25)];

  assert.ok(Math.abs(rms(sine(1, 0.5)) - 0.5 / Math.SQRT2) < 1e-4);
  assert.ok(Math.abs(rms(loudThenQuiet, 0, sampleRate) - 0.5 / Math.SQRT2) < 1e-4);
  assert.ok(Math.abs(rms(loudThenQuiet, sampleRate) - 0.25 / Math.SQRT2) < 1e-4);
});

test('the largest error is found in a signal of any length', () => {
  const expected = sine(5, 0.5);
  const actual = Float32Array.from(expected);
  actual[200000] = expected[200000]! + 0.125;

  assert.equal(maxError(expected, expected), 0);
  assert.ok(Math.abs(maxError(actual, expected) - 0.125) < 1e-6);
});
