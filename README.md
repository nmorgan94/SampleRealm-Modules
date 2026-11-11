# SampleRealm-Modules

Code shared by every SampleRealm plugin, fetched by each at a pinned commit.
[JUCE-Starter](https://github.com/nmorgan94/JUCE-Starter) shows the wiring.

| Folder | Holds |
| --- | --- |
| `licensing/` | A JUCE module: this machine's code, offline licence verification, and activation |
| `cmake/Licensing.cmake` | `add_licensing(<plugin target>)`, which also makes `licensing::fixtures` for the plugin's tests |
| `Tests/` | The module's tests, `LicenceFixtures.h` and the test licences |
| `Tests/plugin/` | Tests built into every plugin's own tests: its company name, and that it never trusts the test key |
| `core/samplerealm/` | Headers a plugin and its wasm engine share: parameter specs and values, the UI info, the transport, the peak meter |
| `cmake/Core.cmake` | `add_plugin_core(<target>)`, which puts them on the include path as `<samplerealm/...>` |
| `cmake/BuildInfo.cmake` | `add_build_info(<target>)`: the plugin repo's commit as `buildCommit` (`<samplerealm/BuildCommit.h>`) |
| `cmake/WasmDemo.cmake` | `add_wasm_demo(SOURCES ...)`: the web demo's engine and page, with the JUCE patch it needs |
| `web/` | TypeScript for a plugin's `webui/`, as the npm package `@samplerealm/modules` |
| `Tests/web/` | Its tests |

## Using it in a plugin

One line in the plugin's `webui/package.json` names the copy every build uses:

```json
"dependencies": {
  "@samplerealm/modules": "github:nmorgan94/SampleRealm-Modules#<full commit id>"
}
```

- The plugin's `cmake/Modules.cmake` (from JUCE-Starter) reads that line and sets
  `SampleRealm-Modules_SOURCE_DIR`. Each `cmake/*.cmake` file here starts with how to call it.
- The trusted keys and the activation page come from the plugin's `LICENCE_KEYS` and `ACTIVATION_URL`
  environment variables. This repo holds neither.
- A plugin's test target links `licensing::fixtures`, which brings `Tests/plugin/` with it.
- Imports name the file: `@samplerealm/modules/demo/localPresets.ts`. `backend.ts` is the editor's
  contract, `demo/` the web demo (`processor.js` is imported with `?url`), `testing/` the set-up
  for a plugin's own tests.
- The demo's stylesheet needs `@source '<path to node_modules>/@samplerealm/modules/web';` for
  Tailwind, and Vite needs `resolve.dedupe: ['react', 'react-dom']` when the package is linked.
- Node won't run TypeScript from `node_modules`, so a plugin's tests run as
  `node --import @samplerealm/modules/testing/typescript.mjs --test ...`.

## Tests

```bash
cmake --workflow --preset test           # Debug
cmake --workflow --preset test-release   # Release, as CI runs
npm ci && npm run typecheck && npm test  # web/
```

## Rules

- A plugin pins a commit id, never a branch: the plugin's cache would keep the branch's first fetch.
  Tags come once the modules settle.
- The machine code recipe and the licence format can't change: shipped plugins and issued licences
  depend on them. `Tests/LicenceTests.cpp` holds the known answer.
- `Tests/fixtures/licence/` is copied from the website. Plugin builds must never trust the test key,
  and its private half must never be copied here.
- `core/` builds for wasm: `juce_core` and `juce_audio_basics` only, and no `juce::String`, `var`,
  `JSON`, printf or `to_chars`.
- To try a change in a plugin before pushing, make its line `"file:../../SampleRealm-Modules"` and
  run `npm install` in its `webui/`.
