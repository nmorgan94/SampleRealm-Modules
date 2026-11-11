# SampleRealm-Modules

Code shared by every SampleRealm plugin, fetched by each at a pinned commit.
[JUCE-Starter](https://github.com/nmorgan94/JUCE-Starter) shows the wiring.

| Folder | Holds |
| --- | --- |
| `licensing/` | A JUCE module: this machine's code, offline licence verification, and activation |
| `cmake/Licensing.cmake` | `add_licensing(<plugin target>)`, which also makes `licensing::fixtures` for the plugin's tests |
| `Tests/` | The module's tests, `LicenceFixtures.h` and the test licences |
| `Tests/plugin/` | Tests built into every plugin's own tests: its company name, and that it never trusts the test key |

## Using it in a plugin

After `juce_add_plugin`:

```cmake
CPMAddPackage(
  NAME SampleRealm-Modules
  GIT_TAG <commit id>
  GITHUB_REPOSITORY nmorgan94/SampleRealm-Modules
  DOWNLOAD_ONLY YES
)

include(${SampleRealm-Modules_SOURCE_DIR}/cmake/Licensing.cmake)
add_licensing(${PROJECT_NAME})
```

- The trusted keys and the activation page come from the plugin's `LICENCE_KEYS` and `ACTIVATION_URL`
  environment variables. This repo holds neither.
- A plugin's test target links `licensing::fixtures`, which brings `Tests/plugin/` with it.

## Tests

```bash
cmake --workflow --preset test           # Debug
cmake --workflow --preset test-release   # Release, as CI runs
```

## Rules

- A plugin pins a commit id, never a branch: the plugin's cache would keep the branch's first fetch.
  Tags come once the modules settle.
- The machine code recipe and the licence format can't change: shipped plugins and issued licences
  depend on them. `Tests/LicenceTests.cpp` holds the known answer.
- `Tests/fixtures/licence/` is copied from the website. Plugin builds must never trust the test key,
  and its private half must never be copied here.
- To try a change in a plugin before pushing, configure the plugin with
  `-DCPM_SampleRealm-Modules_SOURCE=<this folder's full path>`.
