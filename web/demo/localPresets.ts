// A plugin's Source/presets/PresetManager.cpp's rules, with user presets in the browser instead of files
import { isSamePreset, listenerSet, type PresetBackend, type PresetEntry, type PresetStatus } from '../backend.ts';

// S is the plugin's own state, stored beside the entry's fields
type StoredPreset<S> = PresetEntry & S;
export type PresetStorage = Pick<Storage, 'getItem' | 'setItem'>;

// The page's own storage. Undefined where it's blocked, which throws on access, and under Node
export function browserStorage(): PresetStorage | undefined {
  try {
    return window.localStorage;
  } catch {
    return undefined;
  }
}

const INIT: PresetEntry = { name: 'Init', category: '', isFactory: true };

const ENTITIES: Record<string, string> = { quot: '"', apos: "'", lt: '<', gt: '>', amp: '&' };

// XmlElement writes other characters it can't leave as-is by number
const decode = (text: string) =>
  text.replace(/&(?:#(\d+)|(quot|apos|lt|gt|amp));/g, (_, code: string | undefined, entity: string) =>
    code ? String.fromCodePoint(Number(code)) : ENTITIES[entity]!);

const xmlAttributes = (tag: string) =>
  Object.fromEntries([...tag.matchAll(/([\w:-]+)="([^"]*)"/g)].map(([, key, value]) => [key, decode(value!)]));

export const xmlTags = (xml: string, name: string) =>
  [...xml.matchAll(new RegExp(`<${name}\\s[^>]*>`, 'g'))].map(([tag]) => xmlAttributes(tag));

// Reads the files PresetManager writes; there's no DOMParser in node
export function parsePreset<S extends object>(xml: string, parseState: (xml: string) => S): StoredPreset<S> {
  const preset = xmlTags(xml, 'PRESET')[0];
  if (!preset) throw new Error('Not a preset');

  return { ...parseState(xml), name: preset.name ?? '', category: preset.category ?? '', isFactory: true };
}

// juce::String::compareNatural
const collator = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });

const sorted = <T extends PresetEntry>(presets: T[]) =>
  [...presets].sort((a, b) => collator.compare(a.category, b.category) || collator.compare(a.name, b.name));

// juce::File::createLegalFileName
const legalName = (name: string) => name.trim().replace(/["#@,;:<>*^|?\\/]/g, '');

// juce::String::equalsIgnoreCase, and macOS filenames
const sameName = (a: string, b: string) => a.toLowerCase() === b.toLowerCase();

interface Options<S> {
  // A site's demos share its storage
  storageKey: string;
  storage?: PresetStorage;
  // The factory preset files' text
  factory: string[];
  parse(xml: string): S;
  // What Init loads
  init: S;
  capture(): S;
  // Anything missing from the state loads at its default, as in the APVTS
  apply(state: S): void;
}

type LocalPresets = PresetBackend & { markModified(): void };

export function localPresets<S extends object>({ storageKey, storage, factory, parse, init, capture, apply }: Options<S>): LocalPresets {
  const factoryPresets = sorted(factory.map((xml) => parsePreset(xml, parse)));
  let users: StoredPreset<S>[] = [];
  let status: PresetStatus = { current: INIT, modified: false };
  const listeners = listenerSet<[PresetStatus]>();

  const read = () => {
    if (!storage) return;
    try {
      const stored = JSON.parse(storage.getItem(storageKey) ?? '[]') as unknown[];
      // Anything that isn't a named preset is left out
      const presets = stored.filter((entry): entry is StoredPreset<S> => typeof (entry as PresetEntry | null)?.name === 'string');
      users = sorted(presets.map((preset) => ({ ...preset, category: typeof preset.category === 'string' ? preset.category : '', isFactory: false })));
    } catch {
      // Unreadable storage keeps what's already listed
    }
  };

  const write = () => {
    try {
      storage?.setItem(storageKey, JSON.stringify(users.map(({ isFactory: _, ...stored }) => stored)));
    } catch {
      // Private windows can refuse storage; the presets last until the page closes
      storage = undefined;
    }
  };

  const setStatus = ({ name, category, isFactory }: PresetEntry, modified: boolean) => {
    status = { current: { name, category, isFactory }, modified };
    listeners.notify(status);
  };

  const all = () => [...factoryPresets, ...users];
  const find = (preset: PresetEntry) => all().find((candidate) => isSamePreset(candidate, preset));
  const findUser = (name: string) => users.find((preset) => sameName(preset.name, name));

  const checkName = (name: string) => {
    if (!name) return 'Enter a preset name.';
    const factoryMatch = factoryPresets.find((preset) => sameName(preset.name, name));
    return factoryMatch ? `"${factoryMatch.name}" is a factory preset. Choose another name.` : null;
  };

  const load = (preset: StoredPreset<S>) => {
    apply(preset);
    setStatus(preset, false);
  };

  const markModified = () => {
    if (!status.modified) setStatus(status.current, true);
  };

  const step = async (delta: number) => {
    read();
    const presets = all();
    if (presets.length === 0) return;

    const index = presets.findIndex((preset) => isSamePreset(preset, status.current));
    if (index < 0) load(delta > 0 ? presets[0]! : presets[presets.length - 1]!);
    else load(presets[(index + delta + presets.length) % presets.length]!);
  };

  read();

  return {
    status: () => status,
    onStatus: listeners.add,
    list: async () => {
      read();
      return all();
    },
    current: async () => {
      read();
      return find(status.current) ?? null;
    },
    load: async (preset) => {
      read();
      const found = find(preset);
      if (found) load(found);
      return found !== undefined;
    },
    loadInit: async () => load({ ...init, ...INIT }),
    loadNext: () => step(1),
    loadPrevious: () => step(-1),
    exists: async (name) => {
      read();
      return findUser(legalName(name)) !== undefined;
    },
    save: async (name, category) => {
      read();
      const trimmed = legalName(name);
      const error = checkName(trimmed);
      if (error) return error;

      const preset: StoredPreset<S> = { ...capture(), name: trimmed, category, isFactory: false };
      users = sorted([...users.filter((user) => !sameName(user.name, trimmed)), preset]);
      write();
      setStatus(preset, false);
      return null;
    },
    rename: async (preset, newName) => {
      read();
      const trimmed = legalName(newName);
      const target = preset.isFactory ? undefined : findUser(preset.name);
      if (!target) return 'Only user presets can be renamed.';

      const error = checkName(trimmed);
      if (error) return error;

      const existing = findUser(trimmed);
      if (existing && existing !== target) return `A preset called "${trimmed}" already exists.`;

      users = sorted(users.map((user) => (user === target ? { ...user, name: trimmed } : user)));
      write();

      if (isSamePreset(status.current, target)) setStatus({ ...status.current, name: trimmed }, status.modified);
      return null;
    },
    remove: async (preset) => {
      read();
      const target = preset.isFactory ? undefined : findUser(preset.name);
      if (!target) return 'Only user presets can be deleted.';

      users = users.filter((user) => user !== target);
      write();

      if (isSamePreset(status.current, target)) markModified();
      return null;
    },
    markModified,
  };
}
