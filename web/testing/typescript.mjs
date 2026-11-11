// Node won't run TypeScript from node_modules, where a plugin has this package.
// A plugin's tests start with `node --import @samplerealm/modules/testing/typescript.mjs`
import { readFileSync } from 'node:fs';
import { registerHooks, stripTypeScriptTypes } from 'node:module';
import { fileURLToPath } from 'node:url';

// This package's web/ folder
const packageFiles = new URL('..', import.meta.url).href;

// A linked copy is outside node_modules, where Node strips the types itself
if (packageFiles.includes('/node_modules/')) {
  registerHooks({
    load(url, context, nextLoad) {
      if (!url.startsWith(packageFiles) || !url.endsWith('.ts')) return nextLoad(url, context);

      return { format: 'module', shortCircuit: true, source: stripTypeScriptTypes(readFileSync(fileURLToPath(url), 'utf8')) };
    },
  });
}
