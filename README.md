# SampleRealm-Modules

Code shared by every SampleRealm plugin, fetched by each at a pinned commit.
[JUCE-Starter](https://github.com/nmorgan94/JUCE-Starter) shows the wiring.

## Where things are

### C++

| Folder | Holds |
| --- | --- |
| `core/samplerealm/` | Headers the plugin and its wasm engine share: parameter specs and values, UI info, transport, peak meter |
| `licensing/` | JUCE module: machine code, offline licence verification, activation |
| `editor/` | JUCE module: header bar, presets, overlays, activation bar, About, `LabeledSlider`, `LevelMeter`, `createParameterLayout` |
| `webview_editor/` | JUCE module: `WebViewEditor`, the native side of `web/plugin/pluginBackend.ts` |

### CMake

Each file starts with how to call it.

| File | Gives |
| --- | --- |
| `cmake/Core.cmake` | `add_plugin_core(<target>)`: `<samplerealm/...>` and `buildCommit` |
| `cmake/Licensing.cmake` | `add_licensing(<plugin target>)`, and `licensing::fixtures` for its tests |
| `cmake/Editor.cmake` | `add_plugin_editor(<plugin target>)`: `<editor/editor.h>` |
| `cmake/WebUi.cmake` | `add_webview_ui(<plugin target>)`: `webui/` built into the plugin, and `<webview_editor/webview_editor.h>` |
| `cmake/WasmDemo.cmake` | `add_wasm_demo(SOURCES ...)`: the web demo's engine and page |
| `cmake/Tests.cmake` | `add_unit_tests(<plugin target>)`: the plugin's `Tests/` with Catch2 and CTest |
| `cmake/Pluginval.cmake` | `add_pluginval_tests(<plugin target>)`: pluginval on each format |

### Web

The npm package `@samplerealm/modules`. Imports name the file: `@samplerealm/modules/demo/localPresets.ts`.

| Folder | Holds |
| --- | --- |
| `web/backend.ts` | The contract between an editor and what runs it |
| `web/editor/` | The React twins of `editor/`, with the menu, text button, parameter maths and `useParam` |
| `web/plugin/` | `pluginBackend`, the backend inside a WebView |
| `web/demo/` | The web demo: frame, engine loading, worklet backend, local presets (`processor.js` is imported with `?url`) |
| `web/build/` | `webuiConfig`, for the plugin's `webui/vite.config.ts` |
| `web/testing/` | Set-up for a plugin's own web tests |

### Tests

| Folder | Holds |
| --- | --- |
| `Tests/` | The C++ tests, and headers a plugin's tests include: `LicenceFixtures.h`, `EditorFixtures.h`, `PluginTestHelpers.h` |
| `Tests/plugin/` | Tests built into every plugin's tests through `licensing::fixtures`: its company and product names, and that it never trusts the test key |
| `Tests/fixtures/licence/` | The test key and licences, copied from the website |
| `Tests/web/` | The web tests |

```bash
cmake --workflow --preset test           # Debug
cmake --workflow --preset test-release   # Release, as CI runs
npm ci && npm run typecheck && npm test  # web/
```

## Using it in a plugin

One line in the plugin's `webui/package.json` names the copy every build uses:

```json
"dependencies": {
  "@samplerealm/modules": "github:nmorgan94/SampleRealm-Modules#<full commit id>"
}
```

- The plugin's `cmake/Modules.cmake` reads that line and sets `SampleRealm-Modules_SOURCE_DIR`.
- To try a change before pushing, make the line `"file:../../SampleRealm-Modules"` and run
  `npm install` in the plugin's `webui/`.
- `LICENCE_KEYS` and `ACTIVATION_URL` are the plugin's environment variables. This repo holds neither.
- Tailwind needs `@source '<path to node_modules>/@samplerealm/modules/web';` in the demo's
  stylesheet and `.../web/editor` in the plugin's.
- A plugin's web tests run as `node --import @samplerealm/modules/testing/typescript.mjs --test ...`,
  because Node won't run TypeScript from `node_modules`.

### What the plugin supplies to the editor

- Native colours: its LookAndFeel sets the colour IDs of `HeaderBar`, `ModalOverlay`, `ActivationBar`
  and `LevelMeter`, and draws the rotary slider.
- Native fonts: its LookAndFeel implements `EditorFonts`.
- Web colours: its stylesheet defines `--color-` `background`, `widget`, `line`, `fg`, `thumb`,
  `clip`, `header`, `title`, `activation`, `activation-outline`, `overlay-dim`, `overlay-panel`,
  `overlay-outline`, `input`, `input-outline`, `input-focus`, `input-highlight` and `scrollbar`.
- Web fonts: `fonts.ts` assumes Orbitron.
- The manual: `ManualOverlay (markdown)` and `useManual(markdown)`.

## Rules

- A plugin pins a commit id, never a branch: its cache would keep the branch's first fetch.
- The machine code recipe and the licence format can't change: shipped plugins and issued licences
  depend on them. `Tests/LicenceTests.cpp` holds the known answer.
- A licence is for one plugin on one machine: the machine code's salt is
  `<COMPANY_NAME>/<PRODUCT_NAME>/machine-code/v1` and the file `<COMPANY_NAME>/<PRODUCT_NAME>.licence`,
  so neither name can change once the plugin ships.
- Plugin builds must never trust the test key, and its private half must never be copied here.
- `core/` builds for wasm: `juce_core` and `juce_audio_basics` only, and no `juce::String`, `var`,
  `JSON`, printf or `to_chars`.
- A plugin's wasm engine exports `uiInfo()`, `prepare(sampleRate, channels)`, `setParam(index, value)`,
  `channel(index)` (an effect's input is written there, and the output read back),
  `render(numSamples)` and `peak(index)`. An effect adds `setTransport(bpm, playing, ppq)`, a synth
  `midi(status, data1, data2)`. `web/demo/processor.js` and `web/testing/engine.ts` rely on these.
