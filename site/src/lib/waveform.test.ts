import assert from 'node:assert/strict';
import { test } from 'node:test';
import { waveformHeights } from './waveform.ts';

test('represents the amplitude of each section of a sample', () => {
  assert.deepEqual(waveformHeights([new Float32Array([0, 0, 0.5, 0.5, 1, 1])], 3), [4, 50, 96]);
});

test('accounts for both stereo channels and silence', () => {
  assert.deepEqual(waveformHeights([new Float32Array([0, 0]), new Float32Array([0, 1])], 2), [4, 96]);
  assert.deepEqual(waveformHeights([new Float32Array(4)], 2), [4, 4]);
});

test('rejects invalid audio data', () => {
  assert.throws(() => waveformHeights([], 2));
  assert.throws(() => waveformHeights([new Float32Array(4)], 0));
  assert.throws(() => waveformHeights([new Float32Array(4), new Float32Array(2)], 2));
});
