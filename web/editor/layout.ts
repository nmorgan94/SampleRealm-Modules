export interface Rect {
  left: number;
  top: number;
  width: number;
  height: number;
}

// juce::Rectangle::withSizeKeepingCentre
export const centred = (outer: number, inner: number) => Math.trunc((outer - inner) / 2);

// editor/LabeledSlider.h's layOutAboveLabel: the label right under a square control, the pair centred
export function layOutAboveLabel(width: number, height: number): { control: Rect; label: Rect } {
  const LABEL_HEIGHT = 24;
  const size = Math.max(0, Math.min(width, height - LABEL_HEIGHT));
  const top = centred(height, size + LABEL_HEIGHT);
  return {
    control: { left: centred(width, size), top, width: size, height: size },
    label: { left: 0, top: top + size, width, height: LABEL_HEIGHT },
  };
}
