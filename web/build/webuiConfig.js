// A plugin's webui/vite.config.ts. JavaScript, because Node won't run TypeScript from node_modules
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

const TARGETS = ['plugin', 'demo'];

// The engine `cmake --workflow --preset wasm` last built
const localEngine = (enginePath) => ({
  name: 'local-engine',
  configureServer(server) {
    server.middlewares.use('/engine.wasm', (_, res) => {
      res.setHeader('Content-Type', 'application/wasm');
      res.end(readFileSync(enginePath()));
    });
  },
});

export function webuiConfig({ mode, webuiDir, plugins, enginePath }) {
  if (!TARGETS.includes(mode)) throw new Error(`Pass --mode ${TARGETS.join(' or ')}`);

  return {
    root: resolve(webuiDir, 'src', mode),
    publicDir: resolve(webuiDir, 'public', mode),
    // Loaded from the plugin's zip and from the CDN's demo folder
    base: './',
    plugins: [...plugins, localEngine(enginePath)],
    // A linked @samplerealm/modules has its own React installed
    resolve: { dedupe: ['react', 'react-dom'] },
    server: { port: 8099, fs: { allow: [resolve(webuiDir, '..')] } },
    build: {
      outDir: resolve(webuiDir, 'dist', mode),
      emptyOutDir: true,
      assetsInlineLimit: 0,
      modulePreload: { polyfill: false },
      rolldownOptions: {
        output: {
          entryFileNames: 'editor.js',
          assetFileNames: ({ names }) => (names[0]?.endsWith('.css') ? 'editor.css' : '[name][extname]'),
        },
      },
    },
  };
}
