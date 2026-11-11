import assert from 'node:assert/strict';
import { test } from 'node:test';
import { webuiConfig } from '../../web/build/webuiConfig.js';

test('a mode other than plugin or demo is refused', () => {
  const options = { webuiDir: '/plugin/webui', plugins: [], enginePath: () => '/plugin/wasm/engine.wasm' };

  assert.throws(() => webuiConfig({ ...options, mode: 'production' }), /--mode plugin or demo/);
  assert.doesNotThrow(() => webuiConfig({ ...options, mode: 'plugin' }));
  assert.doesNotThrow(() => webuiConfig({ ...options, mode: 'demo' }));
});
