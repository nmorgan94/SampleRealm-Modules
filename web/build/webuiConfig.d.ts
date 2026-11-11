// No Vite types: a linked copy of this package would bring its own, which don't match the plugin's

interface LocalEnginePlugin {
  name: string;
  configureServer(server: {
    middlewares: { use(path: string, handler: (request: unknown, response: { setHeader(name: string, value: string): void; end(body: Uint8Array): void }) => void): void };
  }): void;
}

export interface WebuiConfigOptions<VitePlugin> {
  // `plugin` or `demo`, from --mode
  mode: string;
  // The plugin's webui/ folder
  webuiDir: string;
  // React and Tailwind, from the plugin's own install
  plugins: VitePlugin[];
  // The engine `npm run serve` gives the demo
  enginePath(): string;
}

export function webuiConfig<VitePlugin>(options: WebuiConfigOptions<VitePlugin>): {
  root: string;
  publicDir: string;
  base: string;
  plugins: (VitePlugin | LocalEnginePlugin)[];
  resolve: { dedupe: string[] };
  server: { port: number; fs: { allow: string[] } };
  build: {
    outDir: string;
    emptyOutDir: boolean;
    assetsInlineLimit: number;
    modulePreload: { polyfill: boolean };
    rolldownOptions: {
      output: { entryFileNames: string; assetFileNames(asset: { names: string[] }): string };
    };
  };
};
