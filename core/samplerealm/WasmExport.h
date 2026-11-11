#pragma once

/** Names a function the page can call on the wasm engine: WASM_EXPORT (render) void render (int numSamples) */
#define WASM_EXPORT(name) extern "C" __attribute__ ((export_name (#name)))
