# SampleRealm-Modules

Code shared by every SampleRealm plugin, fetched by each at a pinned commit.
[JUCE-Starter](https://github.com/nmorgan94/JUCE-Starter) shows the wiring.

## Where things are

### C++

| Folder | Holds |
| --- | --- |
| `core/samplerealm/` | Headers the plugin and its wasm engine share: parameter specs and values, UI info, transport, peak meter |
| `licensing/` | JUCE module: machine code, offline licence verification, activation |
| `controls/` | JUCE module: `createParameterLayout`, `LabeledSlider`, `LevelMeter`. Needs none of the modules below |
| `editor/` | JUCE module: header bar, presets, overlays, About, on top of `controls/`. No licensing |
| `activation/` | JUCE module: the activation bar, on `editor/` and `licensing/` |
| `webview_editor/` | JUCE module: `WebViewEditor`, the native side of `web/plugin/pluginBackend.ts` |

### CMake

Each file starts with how to call it.

| File | Gives |
| --- | --- |
| `cmake/Core.cmake` | `add_plugin_core(<target>)`: `<samplerealm/...>` and `buildCommit` |
| `cmake/Licensing.cmake` | `add_licensing(<plugin target>)`, and `licensing::fixtures` for its tests |
| `cmake/Controls.cmake` | `add_plugin_controls(<plugin target>)`: `<controls/controls.h>` |
| `cmake/Editor.cmake` | `add_plugin_editor(<plugin target>)`: `<editor/editor.h>`, with controls |
| `cmake/Activation.cmake` | `add_plugin_activation(<plugin target>)`: `<activation/activation.h>`, with the editor and licensing |
| `cmake/WebUi.cmake` | `add_webview_ui(<plugin target>)`: `webui/` built into the plugin, and `<webview_editor/webview_editor.h>` |
| `cmake/WasmDemo.cmake` | `add_wasm_demo(SOURCES ...)`: the web demo's engine and page |
| `cmake/Tests.cmake` | `add_unit_tests(<plugin target>)`: the plugin's `Tests/` with Catch2 and CTest |
| `cmake/Pluginval.cmake` | `add_pluginval_tests(<plugin target>)`: pluginval on each format |

### Web

The npm package `@samplerealm/modules`. Imports name the file: `@samplerealm/modules/demo/localPresets.ts`.

| Folder | Holds |
| --- | --- |
| `web/backend.ts` | The contract between an editor and what runs it |
| `web/editor/` | The React twins of `controls/` and `editor/`, with the menu, text button, parameter maths and `useParam` |
| `web/plugin/` | `pluginBackend`, the backend inside a WebView |
| `web/demo/` | The web demo: frame, engine loading, worklet backend, local presets (`processor.js` is imported with `?url`) |
| `web/build/` | `webuiConfig`, for the plugin's `webui/vite.config.ts` |
| `web/testing/` | Set-up for a plugin's own web tests |

### Tests

| Folder | Holds |
| --- | --- |
| `Tests/` | The C++ tests, and headers a plugin's tests include: `LicenceFixtures.h`, `EditorFixtures.h`, `PluginTestHelpers.h` |
| `Tests/editor-only/` | The editor built as a plugin without activation has it, with no licensing |
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

### A plugin without the header

- Its native build calls `add_plugin_controls` in place of `add_plugin_editor`, and includes
  `<controls/controls.h>`: no header, presets or overlays are compiled in. Its WebView build still
  gets them, as `webview_editor` is built on `editor`; `add_webview_ui` brings them in, without licensing.
- `WebViewEditor::Plugin` takes `presets` and `meter` as pointers, null by default. Its page then
  uses `editorBackend()`, an `EditorBackend`, in place of `pluginBackend()`, adding `onLevel` if it has a meter.
- `DemoFrame` takes an `overlay`, shown over the editor without being scaled with it.
- `LabeledSlider::labelHeight`, and `LABEL_HEIGHT` in `layout.ts`, are the height it adds under the knob.
- `Plugin::onTick` sends the plugin's own events to the page, and `Plugin::background` is its colour.
  `Plugin::withOptions` adds the plugin's own native functions and initialisation data.
- `LabeledSlider.tsx` takes the plugin's `rotary` drawing, a `unit`, and a `labelClassName` for
  what its LookAndFeel's `drawLabel` adds, such as a box behind the name.
- The name goes above with `setNamePlacement` / `namePlacement="above"`, sized by `setNameHeight` / `nameHeight`.
  Native drag distance and value text are the slider's own; the web's are `pixelsForFullDrag` and `valueText`.
- A choice parameter reads a typed name in any case, then a number, natively and in `parseValue`.
- `engineAt(path)(channels, rate)` takes a sample rate, or `null` for an engine never prepared.
- `HeaderBar` takes a `Title { bold, regular }`, and `HeaderBar.tsx` a `{ bold, regular }`, for a title in two weights.
- `LevelMeter.tsx` takes `channels`, the bars shown before the first level.
- `TestHelpers::process (processor, buffer, message)` plays one MIDI message at the start of the block, for a synth.
- A `toggle (...)` spec is an `AudioParameterBool`; `notAutomatable (...)` around any spec keeps hosts from automating it.
- `performanceControl (...)` around a spec, such as pitch bend, marks it played live: give
  `PresetManager::performanceControlIds (specs)` to `PresetManager::Config::ignoredParameterIds`, so presets don't save it and moving it isn't an edit. The web
  demo's presets do the same from `uiInfo`, where `isPerformanceControl` is there only when true.
- `createParameterLayout (specs, versionHint, floatAttributesFor)` gives a real parameter value text of the plugin's own.
- `WorkletBackend.showValue (id, value)` shows a value the engine set itself, from MIDI say, without sending it back.
- `onEvent (name, listener)` in `pluginBackend.ts` hears what `Plugin::onTick` emits, and
  `initialData (name)` reads what `Plugin::withOptions` adds.

### A plugin without activation

It has no licensing code: it calls `add_plugin_editor` or `add_plugin_controls`, not `add_plugin_activation`,
and leaves out `PLUGIN_REQUIRE_ACTIVATION`, `Plugin::activation` and the activation bar. Its WebView page
is told the bar never shows. A plugin with activation calls `add_plugin_activation` and includes
`<activation/activation.h>`.

### What the plugin supplies to the editor

- Native colours: its LookAndFeel sets the colour IDs of `HeaderBar`, `ModalOverlay`, `ActivationBar`
  and `LevelMeter`, and draws the rotary slider.
- Native fonts: its LookAndFeel implements `EditorFonts`.
- Web colours: its stylesheet defines `--color-` `background`, `widget`, `line`, `fg`, `thumb`,
  `clip`, `header`, `title`, `activation`, `activation-outline`, `overlay-dim`, `overlay-panel`,
  `overlay-outline`, `input`, `input-outline`, `input-focus`, `input-highlight` and `scrollbar`.
  Optional, falling back to those: `button-text`, `meter-background`, `menu`, `menu-header`,
  `menu-highlight` and `menu-highlight-text`.
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
  `render(numSamples)` and, for a plugin with a meter, `peak(index)`. An effect adds `setTransport(bpm, playing, ppq)`, a synth
  `midi(status, data1, data2)`. `web/demo/processor.js` and `web/testing/engine.ts` rely on these.
