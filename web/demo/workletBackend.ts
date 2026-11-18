import { listenerSet, paramLookup, type Level, type PluginBackend, type UiInfo } from '../backend.ts';
import { browserStorage, localPresets, xmlTags, type PresetStorage } from './localPresets.ts';

// The node's port, so tests can stand in for it
export type EnginePort = Pick<MessagePort, 'postMessage'> & { onmessage: ((e: MessageEvent) => void) | null };

// A plugin's Source/shared/Transport.h
export type Transport = { bpm: number; playing: boolean; ppq: number };

// What processor.js posts
export type EngineMessage = { type: string; [key: string]: unknown };

export type WorkletBackend = PluginBackend & {
  attach(port: EnginePort): void;
  // The page plays the host
  setTransport(transport: Transport): void;
  // For a synth's engine: a status byte and up to two data bytes, rounded and kept to 0 to 127.
  // System messages (clock, SysEx) are dropped
  midi(bytes: ArrayLike<number>): void;
  noteOn(note: number, velocity?: number): void;
  noteOff(note: number): void;
  // Shows a value the engine set itself, from MIDI say, without sending it back
  showValue(id: string, value: number): void;
  // An edit to state outside the parameters, as PresetManager hears nonParameterStateChanged
  markModified(): void;
  // A message of the plugin's own to processor.js, such as its 'call'. Kept until the worklet attaches
  post(message: unknown): void;
  // Everything processor.js posts, its 'watch' included. Returns how to stop listening
  onMessage(listener: (data: EngineMessage) => void): () => void;
};

const NOTE_ON = 0x90;
const NOTE_OFF = 0x80;
const FIRST_STATUS = 0x80;
const FIRST_SYSTEM_STATUS = 0xf0;

const dataByte = (value: number) => Math.min(127, Math.max(0, Math.round(value)));

// A preset holds parameter values by id, its type where the plugin's presets are typed, and the plugin's extra state
interface PresetState<Extra = {}> {
  values: Record<string, number>;
  type?: string;
  extra?: Partial<Extra>;
}

export const parseState = (xml: string): PresetState => ({
  values: Object.fromEntries(xmlTags(xml, 'PARAM').map(({ id, value }) => [id, value ? Number(value) : NaN])),
  type: xmlTags(xml, 'PRESET')[0]?.type,
});

interface Options<Extra> {
  // The plugin's own, as a site's demos share its storage
  storageKey: string;
  // The factory preset files' text
  factoryPresets?: string[];
  // The page's own unless given
  storage?: PresetStorage;
  // As PresetManager::Config::typeParameterId: a choice whose value's name types each preset. Init keeps it
  typeParameter?: string;
  // State outside the parameters that presets hold too, stored as the preset's `extra`. Init applies {}
  extraState?: {
    parse(xml: string): Extra;
    capture(): Extra;
    apply(state: Partial<Extra>): void;
  };
}

export function workletBackend<Extra extends object = {}>(
  info: UiInfo,
  { storageKey, factoryPresets = [], storage = browserStorage(), typeParameter, extraState }: Options<Extra>
): WorkletBackend {
  const entry = paramLookup(new Map(info.params.map((param, index) => [param.id, { param, index, value: param.defaultValue, listeners: listenerSet<[number]>() }])));
  const levelListeners = listenerSet<[Level]>();
  const messageListeners = listenerSet<[EngineMessage]>();
  let port: EnginePort | null = null;
  // Sent once the worklet attaches
  let pending: unknown[] = [];

  const post = (message: unknown) => {
    if (port) port.postMessage(message);
    else pending.push(message);
  };

  const showValue = (id: string, value: number) => {
    const found = entry(id);
    found.value = value;
    found.listeners.notify(value);
  };

  const apply = (id: string, value: number) => {
    post({ type: 'param', index: entry(id).index, value });
    showValue(id, value);
  };

  const midi = (bytes: ArrayLike<number>) => {
    const status = bytes[0] ?? 0;
    if (status < FIRST_STATUS || status >= FIRST_SYSTEM_STATUS) return;
    post({ type: 'midi', bytes: [status, dataByte(bytes[1] ?? 0), dataByte(bytes[2] ?? 0)] });
  };

  // The type parameter's entry, where presets are typed
  const typed = typeParameter === undefined ? undefined : entry(typeParameter);
  const currentType = () => typed && (typed.param.choices[Math.round(typed.value)] ?? '');

  const presets = localPresets<PresetState<Extra>>({
    storageKey,
    storage,
    factory: factoryPresets,
    parse: (xml) => ({ ...parseState(xml), extra: extraState?.parse(xml) }),
    init: () => ({ values: typed ? { [typed.param.id]: typed.value } : {} }),
    // As PresetManager::Config::ignoredParameterIds: a performance control isn't saved, so a load resets it
    capture: () => ({
      values: Object.fromEntries(info.params.filter(({ isPerformanceControl }) => !isPerformanceControl).map(({ id }) => [id, entry(id).value])),
      type: currentType(),
      extra: extraState?.capture(),
    }),
    // Ids this engine doesn't have are skipped, as the APVTS skips them. A value that isn't a number loads as the default
    apply: ({ values = {}, extra = {} }) => {
      info.params.forEach(({ id, defaultValue }) => {
        const value = values[id];
        apply(id, typeof value === 'number' && Number.isFinite(value) ? value : defaultValue);
      });
      extraState?.apply(extra);
    },
    type: typed && { current: () => currentType() ?? '', of: (state) => state.type },
  });

  return {
    info,
    running: 'Web demo',
    presets,
    param: (id) => entry(id).param,
    getValue: (id) => entry(id).value,
    setValue: (id, value) => {
      apply(id, value);
      if (!entry(id).param.isPerformanceControl) presets.markModified();
    },
    beginGesture: () => {},
    endGesture: () => {},
    subscribe: (id, listener) => entry(id).listeners.add(listener),
    onLevel: levelListeners.add,
    setTransport: (transport) => post({ type: 'transport', ...transport }),
    midi,
    noteOn: (note, velocity = 100) => midi([NOTE_ON, note, velocity]),
    noteOff: (note) => midi([NOTE_OFF, note, 0]),
    showValue,
    markModified: presets.markModified,
    post,
    onMessage: messageListeners.add,
    attach: (enginePort) => {
      port = enginePort;
      port.onmessage = ({ data }) => {
        if (data.type === 'level') levelListeners.notify({ peaks: data.peaks });
        // A call or watched name the engine couldn't take: nothing else shows it
        else if (data.type === 'error') console.error('The demo engine:', data.message);
        messageListeners.notify(data);
      };
      pending.forEach((message) => enginePort.postMessage(message));
      pending = [];
    },
  };
}
