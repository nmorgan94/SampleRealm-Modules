import { listenerSet, paramLookup, type Level, type PluginBackend, type UiInfo } from '../backend.ts';
import { browserStorage, localPresets, xmlTags, type PresetStorage } from './localPresets.ts';

// The node's port, so tests can stand in for it
export type EnginePort = Pick<MessagePort, 'postMessage'> & { onmessage: ((e: MessageEvent) => void) | null };

// A plugin's Source/shared/Transport.h
export type Transport = { bpm: number; playing: boolean; ppq: number };

export type WorkletBackend = PluginBackend & {
  attach(port: EnginePort): void;
  // The page plays the host
  setTransport(transport: Transport): void;
};

// A preset holds parameter values by id
interface PresetState {
  values: Record<string, number>;
}

const parseState = (xml: string): PresetState => ({
  values: Object.fromEntries(xmlTags(xml, 'PARAM').map(({ id, value }) => [id, value ? Number(value) : NaN])),
});

interface Options {
  // The plugin's own, as a site's demos share its storage
  storageKey: string;
  // The factory preset files' text
  factoryPresets?: string[];
  // The page's own unless given
  storage?: PresetStorage;
}

export function workletBackend(info: UiInfo, { storageKey, factoryPresets = [], storage = browserStorage() }: Options): WorkletBackend {
  const entry = paramLookup(new Map(info.params.map((param, index) => [param.id, { param, index, value: param.defaultValue, listeners: listenerSet<[number]>() }])));
  const levelListeners = listenerSet<[Level]>();
  let port: EnginePort | null = null;
  // Sent once the worklet attaches
  let pending: unknown[] = [];

  const post = (message: unknown) => {
    if (port) port.postMessage(message);
    else pending.push(message);
  };

  const apply = (id: string, value: number) => {
    const found = entry(id);
    post({ type: 'param', index: found.index, value });
    found.value = value;
    found.listeners.notify(value);
  };

  const presets = localPresets<PresetState>({
    storageKey,
    storage,
    factory: factoryPresets,
    parse: parseState,
    init: { values: {} },
    capture: () => ({ values: Object.fromEntries(info.params.map(({ id }) => [id, entry(id).value])) }),
    // Ids this engine doesn't have are skipped, as the APVTS skips them. A value that isn't a number loads as the default
    apply: ({ values = {} }) =>
      info.params.forEach(({ id, defaultValue }) => {
        const value = values[id];
        apply(id, typeof value === 'number' && Number.isFinite(value) ? value : defaultValue);
      }),
  });

  return {
    info,
    running: 'Web demo',
    presets,
    param: (id) => entry(id).param,
    getValue: (id) => entry(id).value,
    setValue: (id, value) => {
      apply(id, value);
      presets.markModified();
    },
    beginGesture: () => {},
    endGesture: () => {},
    subscribe: (id, listener) => entry(id).listeners.add(listener),
    onLevel: levelListeners.add,
    setTransport: (transport) => post({ type: 'transport', ...transport }),
    attach: (enginePort) => {
      port = enginePort;
      port.onmessage = ({ data }) => {
        if (data.type === 'level') levelListeners.notify({ peaks: data.peaks });
      };
      pending.forEach((message) => enginePort.postMessage(message));
      pending = [];
    },
  };
}
