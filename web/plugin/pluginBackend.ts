// The C++ side of these names is a plugin's Source/webview/WebViewEditor.cpp
import { getNativeFunction, getSliderState } from '@juce-framework/webview';
import { listenerSet, paramLookup, type ActivationBackend, type ActivationStatus, type Level, type PluginBackend, type PresetBackend, type PresetEntry, type PresetStatus, type UiInfo } from '../backend.ts';
import { toProportion } from '../editor/params.ts';

// JUCE's type lists only its own initialisation data
type InitialisationData = { uiInfo: string[]; presetStatus: string[]; activationStatus: string[]; running: string[] };

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

export function pluginBackend(): PluginBackend {
  const { uiInfo, presetStatus, activationStatus, running } = window.__JUCE__.initialisationData as unknown as InitialisationData;
  const activation = JSON.parse(activationStatus[0]) as ActivationStatus;
  const info = JSON.parse(uiInfo[0]) as UiInfo;
  const entry = paramLookup(new Map(info.params.map((param) => [param.id, { param, slider: getSliderState(param.id) }])));

  const param = (id: string) => entry(id).param;
  const slider = (id: string) => entry(id).slider;

  return {
    info,
    running: running[0],
    presets: pluginPresets(JSON.parse(presetStatus[0]) as PresetStatus),
    // Its functions exist only where the bar could show
    activation: activation.show ? pluginActivation(activation) : undefined,
    param,
    getValue: (id) => slider(id).getScaledValue(),
    setValue: (id, value) => slider(id).setNormalisedValue(toProportion(param(id), value)),
    beginGesture: (id) => slider(id).sliderDragStarted(),
    endGesture: (id) => slider(id).sliderDragEnded(),
    subscribe: (id, listener) => {
      const state = slider(id);
      const handle = state.valueChangedEvent.addListener(() => listener(state.getScaledValue()));
      return () => state.valueChangedEvent.removeListener(handle);
    },
    onLevel: (listener) => {
      const handle = window.__JUCE__.backend.addEventListener('level', (payload) => listener(payload as Level));
      return () => window.__JUCE__.backend.removeEventListener(handle);
    },
  };
}
