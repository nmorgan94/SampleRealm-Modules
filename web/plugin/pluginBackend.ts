// The C++ side of these names is webview_editor/WebViewEditor.cpp
import { getNativeFunction, getSliderState, getToggleState } from '@juce-framework/webview';
import { listenerSet, paramLookup, type ActivationBackend, type ActivationStatus, type EditorBackend, type Level, type ParamInfo, type PluginBackend, type PresetBackend, type PresetEntry, type PresetStatus, type UiInfo } from '../backend.ts';
import { toProportion } from '../editor/params.ts';

// JUCE's type lists only its own initialisation data. presetStatus is there when the plugin has presets
type InitialisationData = { uiInfo: string[]; presetStatus?: string[]; activationStatus: string[]; running: string[] };

function pluginActivation(initial: ActivationStatus): ActivationBackend {
  let status = initial;
  const listeners = listenerSet<[ActivationStatus]>();

  const update = (payload: unknown) => {
    status = payload as ActivationStatus;
    listeners.notify(status);
  };

  const call = (name: string) => getNativeFunction(name)();

  // Activated or not, the bar follows what the plugin now says
  const message = async (result: Promise<unknown>) => {
    const said = ((await result) as string) || null;
    update(await call('activationStatus'));
    return said;
  };

  window.__JUCE__.backend.addEventListener('activationStatus', update);

  return {
    status: () => status,
    onStatus: listeners.add,
    openBrowser: () => void call('activationOpenBrowser'),
    paste: () => message(call('activationPaste')),
    importFile: () => message(call('activationImport')),
  };
}

function pluginPresets(initial: PresetStatus): PresetBackend {
  let status = initial;
  const listeners = listenerSet<[PresetStatus]>();

  const update = (payload: unknown) => {
    status = payload as PresetStatus;
    listeners.notify(status);
  };

  const call = (name: string, ...args: unknown[]) => getNativeFunction(name)(...args);

  // Asked again once listening, in case a change came first
  window.__JUCE__.backend.addEventListener('presetStatus', update);
  call('presetStatus').then(update);
  const error = async (result: Promise<unknown>) => ((await result) as string) || null;

  return {
    status: () => status,
    onStatus: listeners.add,
    list: async () => (await call('presetList')) as PresetEntry[],
    current: async () => ((await call('presetCurrent')) as PresetEntry | null | undefined) ?? null,
    load: async (preset) => (await call('presetLoad', preset)) === true,
    loadInit: async () => void (await call('presetInit')),
    loadNext: async () => void (await call('presetStep', 1)),
    loadPrevious: async () => void (await call('presetStep', -1)),
    exists: async (name) => (await call('presetExists', name)) as boolean,
    save: (name, category) => error(call('presetSave', name, category)),
    rename: (preset, newName) => error(call('presetRename', preset, newName)),
    remove: (preset) => error(call('presetDelete', preset)),
    showFolder: () => void call('presetShowFolder'),
  };
}

type Control = {
  get(): number;
  set(value: number): void;
  begin(): void;
  end(): void;
  onChange(listener: () => void): () => void;
};

type ValueChangedEvent = ReturnType<typeof getSliderState>['valueChangedEvent'];

const listen = (event: ValueChangedEvent) => (listener: () => void) => {
  const handle = event.addListener(listener);
  return () => event.removeListener(handle);
};

// A WebToggleButtonRelay for toggles, a WebSliderRelay for the rest
function control(param: ParamInfo): Control {
  if (param.kind === 'toggle') {
    const state = getToggleState(param.id);
    return {
      get: () => (state.getValue() ? 1 : 0),
      set: (value) => state.setValue(value >= 0.5),
      begin: () => {},
      end: () => {},
      onChange: listen(state.valueChangedEvent),
    };
  }

  const state = getSliderState(param.id);
  return {
    get: () => state.getScaledValue(),
    set: (value) => state.setNormalisedValue(toProportion(param, value)),
    begin: () => state.sliderDragStarted(),
    end: () => state.sliderDragEnded(),
    onChange: listen(state.valueChangedEvent),
  };
}

const initialisationData = () => window.__JUCE__.initialisationData as unknown as InitialisationData;

// What WebViewEditor emits, a plugin's own events from Plugin::onTick included. Returns how to stop listening
export const onEvent = <Payload>(name: string, listener: (payload: Payload) => void) => {
  const handle = window.__JUCE__.backend.addEventListener(name, (payload) => listener(payload as Payload));
  return () => window.__JUCE__.backend.removeEventListener(handle);
};

// For a plugin whose WebViewEditor was given a meter
export const onLevel = (listener: (level: Level) => void) => onEvent('level', listener);

// For a plugin whose WebViewEditor was given no presets
export function editorBackend(data = initialisationData()): EditorBackend {
  const { uiInfo, activationStatus, running } = data;
  const activation = JSON.parse(activationStatus[0]) as ActivationStatus;
  const info = JSON.parse(uiInfo[0]) as UiInfo;
  const entry = paramLookup(new Map(info.params.map((param) => [param.id, { param, control: control(param) }])));

  const controlOf = (id: string) => entry(id).control;

  return {
    info,
    running: running[0],
    // Its functions exist only where the bar could show
    activation: activation.show ? pluginActivation(activation) : undefined,
    param: (id) => entry(id).param,
    getValue: (id) => controlOf(id).get(),
    setValue: (id, value) => controlOf(id).set(value),
    beginGesture: (id) => controlOf(id).begin(),
    endGesture: (id) => controlOf(id).end(),
    subscribe: (id, listener) => {
      const found = controlOf(id);
      return found.onChange(() => listener(found.get()));
    },
  };
}

export function pluginBackend(): PluginBackend {
  const data = initialisationData();
  if (!data.presetStatus) throw new Error('The plugin has no presets: its page uses editorBackend()');

  return { ...editorBackend(data), presets: pluginPresets(JSON.parse(data.presetStatus[0]) as PresetStatus), onLevel };
}
