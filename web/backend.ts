type ParamKind = 'real' | 'integer' | 'choice' | 'toggle';

export interface ParamInfo {
  id: string;
  name: string;
  kind: ParamKind;
  min: number;
  max: number;
  defaultValue: number;
  step: number;
  skew: number;
  unit: string;
  choices: string[];
}

export interface UiInfo {
  version: string;
  commit: string;
  build: 'Debug' | 'Release';
  params: ParamInfo[];
}

// Linear peak per channel since the last level
export interface Level {
  peaks: number[];
}

export interface PresetEntry {
  name: string;
  category: string;
  isFactory: boolean;
}

export interface PresetStatus {
  current: PresetEntry;
  modified: boolean;
}

// Mirrors editor/PresetManager.h
export interface PresetBackend {
  status(): PresetStatus;
  onStatus(listener: (status: PresetStatus) => void): () => void;
  // Factory then user presets, rescanning the user ones
  list(): Promise<PresetEntry[]>;
  // The current preset if it's listed
  current(): Promise<PresetEntry | null>;
  // Resolves to whether it loaded
  load(preset: PresetEntry): Promise<boolean>;
  loadInit(): Promise<void>;
  loadNext(): Promise<void>;
  loadPrevious(): Promise<void>;
  exists(name: string): Promise<boolean>;
  // Each resolves to an error message, or null
  save(name: string, category: string): Promise<string | null>;
  rename(preset: PresetEntry, newName: string): Promise<string | null>;
  remove(preset: PresetEntry): Promise<string | null>;
  // Only where presets are files
  showFolder?(): void;
}

export interface ActivationStatus {
  // False once activated
  show: boolean;
  // As shown: K7Q2-M9XH-4TBV-8RNC
  machine: string;
}

// Mirrors editor/ActivationActions.h
export interface ActivationBackend {
  status(): ActivationStatus;
  onStatus(listener: (status: ActivationStatus) => void): () => void;
  openBrowser(): void;
  // Each resolves to a message for the user, or null
  paste(): Promise<string | null>;
  importFile(): Promise<string | null>;
}

export const isSamePreset = (a: PresetEntry, b: PresetEntry) => a.name === b.name && a.isFactory === b.isFactory;

// What every editor has. A plugin with the header's presets and meter has a PluginBackend
export interface EditorBackend {
  info: UiInfo;
  // "VST3 in Ableton Live", or where else the editor is running
  running: string;
  // Only in a plugin built to require activation
  activation?: ActivationBackend;
  param(id: string): ParamInfo;
  // Real values, not 0..1
  getValue(id: string): number;
  setValue(id: string, value: number): void;
  beginGesture(id: string): void;
  endGesture(id: string): void;
  subscribe(id: string, listener: (value: number) => void): () => void;
}

export interface PluginBackend extends EditorBackend {
  presets: PresetBackend;
  onLevel(listener: (level: Level) => void): () => void;
}

export const paramLookup = <T>(entries: Map<string, T>) => (id: string) => {
  const found = entries.get(id);
  if (!found) throw new Error(`No parameter ${id}`);
  return found;
};

export const listenerSet = <Args extends unknown[]>() => {
  const listeners = new Set<(...args: Args) => void>();
  return {
    add: (listener: (...args: Args) => void) => {
      listeners.add(listener);
      return () => void listeners.delete(listener);
    },
    notify: (...args: Args) => listeners.forEach((listener) => listener(...args)),
  };
};
