// Stand-ins for a plugin's preset tests
import type { PresetStorage } from '../demo/localPresets.ts';

// The browser's storage, in memory. `items` is what's stored
export function memoryStorage(initial: Record<string, string> = {}): PresetStorage & { items: Map<string, string> } {
  const items = new Map(Object.entries(initial));
  return { items, getItem: (key) => items.get(key) ?? null, setItem: (key, value) => void items.set(key, value) };
}

// A preset file as PresetManager writes it
export const presetXml = (name: string, category: string, values: Record<string, number>) =>
  `<?xml version="1.0" encoding="UTF-8"?>
<PRESET name="${name}" category="${category}" author="Test" formatVersion="1">
  <Parameters>
${Object.entries(values).map(([id, value]) => `    <PARAM id="${id}" value="${value}"/>`).join('\n')}
  </Parameters>
</PRESET>`;
