const LEVEL_EVERY_BLOCKS = 8;

class EngineProcessor extends AudioWorkletProcessor {
  // `watch` names exports of the plugin's own, taking nothing: when one's value changes after a block, all are posted as 'watch'.
  // `frames` is { name, size, perSecond }: the export `name` takes the seconds since its last call and points at
  // as many floats as the export `size` gives, posted as 'frame' about `perSecond` times a second
  constructor({ processorOptions: { wasm, channels, watch = [], frames } }) {
    super();

    try {
      // WASI imports are never called
      const stub = new Proxy({}, { get: () => () => 0 });
      const instance = new WebAssembly.Instance(new WebAssembly.Module(wasm), new Proxy({}, { get: () => stub }));
      this.engine = instance.exports;
      this.engine._initialize?.();
      this.engine.prepare(sampleRate, channels);
      this.watch = watch;
      this.watchedExports = watch.map((name) => this.exportNamed(name));
      this.watched = this.watchedExports.map((watchedExport) => watchedExport());

      if (frames) {
        this.frameExport = this.exportNamed(frames.name);
        this.frameSize = this.exportNamed(frames.size)();
        if (!(this.frameSize > 0)) throw new Error(`The engine's "${frames.size}" gave a frame of ${this.frameSize} floats`);
        if (!(frames.perSecond > 0)) throw new Error(`A frame can't be posted ${frames.perSecond} times a second`);
        // A block is 128 samples at any sample rate
        this.blocksPerFrame = Math.max(1, Math.round(sampleRate / (frames.perSecond * 128)));
        this.blocksSinceFrame = 0;
      }
    } catch (error) {
      this.engine = null;
      this.postError(error);
      return;
    }

    this.channels = channels;
    this.blocks = 0;
    this.lastSilent = false;
    this.views = [];

    this.port.onmessage = ({ data }) => {
      if (data.type === 'param') this.engine.setParam(data.index, data.value);
      // An effect's engine has no midi, a synth's no transport
      else if (data.type === 'transport') this.engine.setTransport?.(data.bpm, data.playing, data.ppq);
      else if (data.type === 'midi') this.engine.midi?.(data.bytes[0], data.bytes[1], data.bytes[2]);
      else if (data.type === 'call') this.call(data);
    };

    this.port.postMessage({ type: 'ready' });
  }

  postError(error) {
    this.port.postMessage({ type: 'error', message: String(error) });
  }

  exportNamed(name) {
    const found = this.engine[name];
    if (typeof found !== 'function') throw new Error(`The engine has no export "${name}"`);
    return found;
  }

  // An export of the plugin's own: { name, args, floats: { at, values } }. The floats are written where the export `at` points first
  call({ name, args = [], floats }) {
    try {
      const called = this.exportNamed(name);
      if (floats) new Float32Array(this.engine.memory.buffer, this.exportNamed(floats.at)(), floats.values.length).set(floats.values);
      called(...args);
    } catch (error) {
      this.postError(error);
    }
  }

  postWatched() {
    this.port.postMessage({ type: 'watch', values: Object.fromEntries(this.watch.map((name, index) => [name, this.watched[index]])) });
  }

  view(channel, frames) {
    const pointer = this.engine.channel(channel);
    const current = this.views[channel];
    // Only changes if memory grows
    if (current?.buffer !== this.engine.memory.buffer || current.byteOffset !== pointer || current.length !== frames) {
      this.views[channel] = new Float32Array(this.engine.memory.buffer, pointer, frames);
    }
    return this.views[channel];
  }

  process([input], [output]) {
    if (!this.engine) return false;

    const frames = output[0].length;

    for (let channel = 0; channel < this.channels; channel++) {
      // No input while the loop is paused, and never for a synth
      if (input?.[channel]) this.view(channel, frames).set(input[channel]);
      else this.view(channel, frames).fill(0);
    }

    this.engine.render(frames);

    output.forEach((channel, index) => channel.set(this.view(Math.min(index, this.channels - 1), frames)));

    // Nothing is allocated here unless a value changed
    let hasChanged = false;

    for (let index = 0; index < this.watchedExports.length; index++) {
      const value = this.watchedExports[index]();
      // NaN is the same as NaN
      if (!Object.is(value, this.watched[index])) hasChanged = true;
      this.watched[index] = value;
    }

    if (hasChanged) this.postWatched();

    if (this.frameExport && ++this.blocksSinceFrame >= this.blocksPerFrame) {
      const pointer = this.frameExport((this.blocksSinceFrame * frames) / sampleRate);
      this.blocksSinceFrame = 0;
      // A copy, handed over rather than cloned
      const frame = new Float32Array(this.engine.memory.buffer, pointer, this.frameSize).slice();
      this.port.postMessage({ type: 'frame', frame }, [frame.buffer]);
    }

    // An engine without a meter has no peak
    if (this.engine.peak && ++this.blocks % LEVEL_EVERY_BLOCKS === 0) {
      const peaks = Array.from({ length: this.channels }, (_, channel) => this.engine.peak(channel));
      const silent = peaks.every((peak) => peak === 0);
      if (!silent || !this.lastSilent) this.port.postMessage({ type: 'level', peaks });
      this.lastSilent = silent;
    }

    return true;
  }
}

registerProcessor('engine-processor', EngineProcessor);
