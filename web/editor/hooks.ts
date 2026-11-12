import { useCallback, useEffect, useState } from 'react';
import type { EditorBackend } from '../backend.ts';
import { snap } from './params.ts';

// Values are snapped as juce::Slider::setValue does: the host sends floats, so -12.3 arrives as -12.300000190734863
export function useParam(backend: EditorBackend, id: string) {
  const current = useCallback(() => snap(backend.param(id), backend.getValue(id)), [backend, id]);
  const [value, setLocal] = useState(current);

  useEffect(() => {
    setLocal(current());
    return backend.subscribe(id, (next) => setLocal(snap(backend.param(id), next)));
  }, [backend, id, current]);

  const set = useCallback(
    (next: number) => {
      setLocal(next);
      backend.setValue(id, next);
    },
    [backend, id]
  );

  const commit = useCallback(
    (next: number) => {
      if (next === current()) return;
      backend.beginGesture(id);
      set(next);
      backend.endGesture(id);
    },
    [backend, id, set, current]
  );

  return { value, set, commit, param: backend.param(id) };
}
